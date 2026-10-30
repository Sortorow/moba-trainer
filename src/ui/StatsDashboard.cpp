#include "ui/StatsDashboard.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "data/GameData.h"
#include "imgui.h"
#include "implot.h"
#include "ui/RecordFormat.h"
#include "ui/UiScale.h"

namespace moba {
namespace {

// 配色（data viz の基準パレットの暗い画面用。先頭の 3 色は、色覚の多様性も含めて見分けられることを検証済みの組）
const ImVec4 kSeriesPlay{0x39 / 255.0f, 0x87 / 255.0f, 0xe5 / 255.0f, 1.0f};     // 青: プレイごと・棒グラフ
const ImVec4 kSeriesAverage{0xd9 / 255.0f, 0x59 / 255.0f, 0x26 / 255.0f, 1.0f};  // 橙: 移動平均
const ImVec4 kSeriesBest{0x19 / 255.0f, 0x9e / 255.0f, 0x70 / 255.0f, 1.0f};     // 緑青: 自己ベストの更新
const ImVec4 kSurface{0x1a / 255.0f, 0x1a / 255.0f, 0x19 / 255.0f, 1.0f};        // グラフの背景
const ImVec4 kGrid{0x2c / 255.0f, 0x2c / 255.0f, 0x2a / 255.0f, 1.0f};           // 目盛りの線
const ImVec4 kAxis{0x38 / 255.0f, 0x38 / 255.0f, 0x35 / 255.0f, 1.0f};           // 軸
const ImVec4 kInkMuted{0x89 / 255.0f, 0x87 / 255.0f, 0x81 / 255.0f, 1.0f};       // 補助の文字
const ImVec4 kInkSecondary{0xc3 / 255.0f, 0xc2 / 255.0f, 0xb7 / 255.0f, 1.0f};   // 軸の文字
const ImVec4 kGood{0x0c / 255.0f, 0xa3 / 255.0f, 0x0c / 255.0f, 1.0f};           // 状態: よくなった
const ImVec4 kCritical{0xd0 / 255.0f, 0x3b / 255.0f, 0x3b / 255.0f, 1.0f};       // 状態: 悪くなった

constexpr ImPlotFlags kPlotFlags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;
constexpr ImPlotAxisFlags kAxisFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoMenus;

/// 成績の名前（「ドッジ率」/「ヒット率」）
const char* scoreName(GameMode mode) { return mode == GameMode::SkillDodge ? "ドッジ率" : "ヒット率"; }

// 記録の表・リザルト画面と共通の表示（ui/RecordFormat.h）
using ui::difficultyLabel;
using ui::percentText;
using ui::shortTime;

/// YYYYMMDD →「10/25」
std::string shortDate(int date) { return std::format("{}/{}", date / 100 % 100, date % 100); }

/// 選択肢 1 つ（値と表示名）
template <class T>
struct Option {
    T value;
    std::string label;
};

/// 絞り込みのコンボボックス。選択肢の先頭は「すべて」にする
template <class T>
void filterCombo(const char* label, T& current, const std::vector<Option<T>>& options, float width) {
    std::string preview = options.empty() ? std::string("-") : options.front().label;
    for (const Option<T>& option : options) {
        if (option.value == current) preview = option.label;
    }
    ImGui::SetNextItemWidth(width);
    if (!ImGui::BeginCombo(label, preview.c_str())) return;
    for (const Option<T>& option : options) {
        if (ImGui::Selectable(option.label.c_str(), option.value == current)) current = option.value;
    }
    ImGui::EndCombo();
}

/// 絞り込みの欄（1 行に並べる）
void drawFilters(StatsFilter& filter, const GameData& data) {
    // 1 行に 6〜7 個並べるので、画面の幅から 1 つ分の幅を決める（最大 170）
    const float width = std::min(ui::px(170.0f), ImGui::GetContentRegionAvail().x / 7.2f);

    std::vector<Option<std::string>> characters{{"", "すべてのキャラ"}};
    for (const CharacterDef* def : data.characterList()) characters.push_back({def->id, def->displayName});
    filterCombo("##character", filter.characterId, characters, width);

    ImGui::SameLine();
    std::vector<Option<std::string>> stages{{"", "すべてのステージ"}};
    for (const StageDef* stage : data.stageList()) stages.push_back({stage->id, stage->displayName});
    filterCombo("##stage", filter.stageId, stages, width);

    ImGui::SameLine();
    std::vector<Option<int>> levels{{0, filter.mode == GameMode::SkillDodge ? "敵の賢さ: すべて" : "的の賢さ: すべて"}};
    if (filter.mode == GameMode::SkillDodge) {
        for (std::size_t i = 0; i < data.ai().levels.size(); ++i)
            levels.push_back({static_cast<int>(i) + 1, std::format("{}（{}）", i + 1, data.ai().levels[i].name)});
    } else {
        for (std::size_t i = 0; i < data.ai().skillShot.levels.size(); ++i)
            levels.push_back(
                {static_cast<int>(i) + 1, std::format("{}（{}）", i + 1, data.ai().skillShot.levels[i].name)});
    }
    filterCombo("##level", filter.level, levels, width * 1.2f);

    ImGui::SameLine();
    if (filter.mode == GameMode::SkillDodge) {
        std::vector<Option<int>> counts{{0, "敵の数: すべて"}};
        for (int n = 1; n <= 5; ++n) counts.push_back({n, std::format("敵 {} 体", n)});
        filterCombo("##enemies", filter.enemyCount, counts, width * 0.8f);
    } else {
        std::vector<Option<float>> sizes{{0.0f, "大きさ: すべて"}};
        for (const TargetSize& size : data.ai().skillShot.sizes)
            sizes.push_back({size.radius, std::format("{}（{:.0f}）", size.name, size.radius)});
        filterCombo("##size", filter.targetRadius, sizes, width * 0.9f);
        ImGui::SameLine();
        std::vector<Option<float>> speeds{{0.0f, "速さ: すべて"}};
        for (const float speed : data.ai().skillShot.moveSpeeds)
            speeds.push_back({speed, std::format("速さ {:.0f}", speed)});
        filterCombo("##speed", filter.targetMoveSpeed, speeds, width * 0.8f);
    }

    ImGui::SameLine();
    const std::vector<Option<int>> periods{{0, std::format("すべて（{} 日分）", kResultKeepDays)},
                                           {7, "直近 7 日"},
                                           {30, "直近 30 日"},
                                           {90, "直近 90 日"}};
    filterCombo("##period", filter.periodDays, periods, width);
}

/// 数字の欄 1 つ（見出し・大きな数字・補足）。補足の色を変える時は noteColor を渡す
void drawTile(const char* id, const char* title, const std::string& value, const std::string& note, float width,
              const ImVec4* noteColor = nullptr) {
    ImGui::BeginChild(id, ImVec2{width, ui::px(104.0f)}, ImGuiChildFlags_Borders);
    ImGui::TextColored(kInkMuted, "%s", title);
    ImGui::PushFont(nullptr, 34.0f);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopFont();
    if (noteColor) ImGui::TextColored(*noteColor, "%s", note.c_str());
    else ImGui::TextColored(kInkSecondary, "%s", note.c_str());
    ImGui::EndChild();
}

/// 数字の欄（プレイ回数・平均・自己ベスト・直近 7 日の比較）
void drawTiles(const StatsSummary& s, GameMode mode) {
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float width = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;

    const int minutes = static_cast<int>(std::lround(s.playedSeconds / 60.0f));
    drawTile("plays", "プレイ回数", std::format("{} 回", s.plays), std::format("合計 {} 分", minutes), width);
    ImGui::SameLine();
    drawTile("average", std::format("平均の{}", scoreName(mode)).c_str(), percentText(s.total),
             std::format("{} / {}", s.total.count, s.total.total), width);
    ImGui::SameLine();
    if (s.best) {
        const ResultScore best = scoreOf(s.best->result);
        drawTile("best", "自己ベスト", percentText(best),
                 std::format("{} / {}・{}", best.count, best.total, shortTime(s.best->playedAt)), width);
    } else {
        drawTile("best", "自己ベスト", "-", std::format("総数 {} 以上のプレイがまだありません", kMinTotalForBest),
                 width);
    }
    ImGui::SameLine();
    // 直近 7 日とその前の 7 日の差（ポイント）。色だけでなく ▲▼ と言葉でも示す
    if (s.recentPlays > 0 && s.previousPlays > 0 && s.recent.total > 0 && s.previous.total > 0) {
        const float diff = (s.recent.rate() - s.previous.rate()) * 100.0f;
        const bool up = diff >= 0.0f;
        const std::string note =
            std::format("{} 前の 7 日より {}{:.1f} ポイント", up ? "▲" : "▼", up ? "+" : "-", std::fabs(diff));
        drawTile("recent", std::format("直近 7 日の{}", scoreName(mode)).c_str(), percentText(s.recent), note, width,
                 up ? &kGood : &kCritical);
    } else {
        drawTile("recent", std::format("直近 7 日の{}", scoreName(mode)).c_str(), percentText(s.recent),
                 s.recentPlays > 0 ? "前の 7 日の記録がありません" : "直近 7 日の記録がありません", width);
    }
}

/// 結果 1 件の説明（グラフのツールチップ用）
void resultTooltip(const SavedResult& saved, const char* heading) {
    const ResultScore score = scoreOf(saved.result);
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(heading);
    ImGui::Text("%s  %s（%d / %d）", saved.playedAt.c_str(), percentText(score).c_str(), score.count, score.total);
    ImGui::TextColored(kInkSecondary, "%s ／ %s ／ %s", saved.result.characterName.c_str(),
                       saved.result.stageName.c_str(), difficultyLabel(saved.result).c_str());
    ImGui::EndTooltip();
}

/// 成績の推移（x = 何回目のプレイか、y = 成績）。プレイごとの点・直近 10 回の平均・自己ベストを更新したプレイ
/// 総数が 0 のプレイ（スキルを 1 回も使わなかったなど）は、割合が無いので描かない
void drawTrendPlot(const std::vector<const SavedResult*>& results, GameMode mode, ImVec2 size) {
    std::vector<const SavedResult*> plays;
    for (const SavedResult* saved : results) {
        if (scoreOf(saved->result).total > 0) plays.push_back(saved);
    }
    std::vector<double> xs;
    std::vector<double> ys;
    std::vector<float> rates;
    for (std::size_t i = 0; i < plays.size(); ++i) {
        const float rate = scoreOf(plays[i]->result).rate() * 100.0f;
        xs.push_back(static_cast<double>(i + 1));
        ys.push_back(rate);
        rates.push_back(rate);
    }
    const std::vector<float> average = movingAverage(rates, kMovingAverageWindow);
    const std::vector<double> averageYs(average.begin(), average.end());
    std::vector<double> bestXs;
    std::vector<double> bestYs;
    for (const std::size_t i : personalBestUpdates(plays)) {
        bestXs.push_back(xs[i]);
        bestYs.push_back(ys[i]);
    }

    const std::string title = std::format("{}の推移", scoreName(mode));
    if (!ImPlot::BeginPlot(title.c_str(), size, kPlotFlags)) return;
    ImPlot::SetupAxis(ImAxis_X1, "何回目のプレイか", kAxisFlags);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, kAxisFlags);
    // 100% の点が上端で切れないように、少しだけ上に余白を取る（目盛りは 100% まで）
    ImPlot::SetupAxesLimits(0.5, std::max(10.5, static_cast<double>(plays.size()) + 0.5), 0.0, 108.0,
                            ImPlotCond_Always);
    static const double kPercentTicks[] = {0, 20, 40, 60, 80, 100};
    static const char* const kPercentLabels[] = {"0%", "20%", "40%", "60%", "80%", "100%"};
    ImPlot::SetupAxisTicks(ImAxis_Y1, kPercentTicks, 6, kPercentLabels);
    ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);

    ImPlotSpec playSpec;
    playSpec.Marker = ImPlotMarker_Circle;
    playSpec.MarkerSize = ui::px(4.0f);
    playSpec.MarkerFillColor = kSeriesPlay;
    playSpec.MarkerLineColor = kSurface;  // 重なった点を見分けられるように、背景色の縁を付ける
    playSpec.LineColor = kSeriesPlay;
    playSpec.LineWeight = ui::px(1.0f);
    ImPlot::PlotScatter("プレイごと", xs.data(), ys.data(), static_cast<int>(xs.size()), playSpec);

    ImPlotSpec averageSpec;
    averageSpec.LineColor = kSeriesAverage;
    averageSpec.LineWeight = ui::px(2.0f);
    const std::string averageLabel = std::format("直近 {} 回の平均", kMovingAverageWindow);
    ImPlot::PlotLine(averageLabel.c_str(), xs.data(), averageYs.data(), static_cast<int>(xs.size()), averageSpec);

    ImPlotSpec bestSpec;
    bestSpec.Marker = ImPlotMarker_Diamond;
    bestSpec.MarkerSize = ui::px(7.0f);
    bestSpec.MarkerFillColor = kSeriesBest;
    bestSpec.MarkerLineColor = kSurface;
    bestSpec.LineColor = kSeriesBest;
    bestSpec.LineWeight = ui::px(2.0f);
    ImPlot::PlotScatter("自己ベストを更新", bestXs.data(), bestYs.data(), static_cast<int>(bestXs.size()), bestSpec);

    // 最新の自己ベストにだけ、数字を添える（すべての点には付けない）
    if (!bestXs.empty()) {
        const std::string label = std::format("自己ベスト {:.1f}%", bestYs.back());
        // 上端に近ければ点の下に、右端に近ければ点の左に出す（枠からはみ出さないように）
        const float offsetY = bestYs.back() > 90.0 ? ui::px(18.0f) : -ui::px(16.0f);
        const bool nearRight = bestXs.back() > static_cast<double>(plays.size()) * 0.8;
        const float offsetX = nearRight ? -(ImGui::CalcTextSize(label.c_str()).x * 0.5f + ui::px(10.0f)) : 0.0f;
        ImPlot::PlotText(label.c_str(), bestXs.back(), bestYs.back(), ImVec2{offsetX, offsetY});
    }

    // カーソルに最も近いプレイの説明
    if (ImPlot::IsPlotHovered() && !plays.empty()) {
        const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        const long index = std::lround(mouse.x) - 1;
        if (index >= 0 && index < static_cast<long>(plays.size())) {
            const std::size_t i = static_cast<std::size_t>(index);
            ImPlotSpec ring;
            ring.Marker = ImPlotMarker_Circle;
            ring.MarkerSize = ui::px(7.0f);
            ring.MarkerFillColor = ImVec4{0, 0, 0, 0};
            ring.MarkerLineColor = ImVec4{1, 1, 1, 0.9f};
            ring.LineWeight = ui::px(2.0f);
            ring.Flags = ImPlotItemFlags_NoLegend;
            ImPlot::PlotScatter("##hover", &xs[i], &ys[i], 1, ring);
            const std::string heading =
                std::format("{} 回目（直近 {} 回の平均 {:.1f}%）", i + 1, kMovingAverageWindow, average[i]);
            resultTooltip(*plays[i], heading.c_str());
        }
    }
    ImPlot::EndPlot();
}

/// 1 色の棒グラフ（x = 項目の番号、y = 値）。カーソルを乗せた棒の説明を tooltip で出す
/// @param labels 項目の名前（x 軸の目盛り）
/// @param tooltip 番号を受け取り、説明の文字を返す
template <class Tooltip>
void drawBarPlot(const char* title, const std::vector<std::string>& labels, const std::vector<double>& values,
                 double yMax, const char* yFormat, ImVec2 size, Tooltip tooltip) {
    if (!ImPlot::BeginPlot(title, size, kPlotFlags | ImPlotFlags_NoLegend)) return;
    ImPlot::SetupAxis(ImAxis_X1, nullptr, kAxisFlags);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr, kAxisFlags);
    const double count = static_cast<double>(std::max<std::size_t>(labels.size(), 1));
    ImPlot::SetupAxesLimits(-0.6, count - 0.4, 0.0, yMax, ImPlotCond_Always);
    ImPlot::SetupAxisFormat(ImAxis_Y1, yFormat);
    // 目盛りの名前は、多すぎる時は間引く（最大 10 個）
    std::vector<double> ticks;
    std::vector<const char*> tickLabels;
    const std::size_t step = std::max<std::size_t>(1, (labels.size() + 9) / 10);
    for (std::size_t i = 0; i < labels.size(); i += step) {
        ticks.push_back(static_cast<double>(i));
        tickLabels.push_back(labels[i].c_str());
    }
    if (!ticks.empty())
        ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), static_cast<int>(ticks.size()), tickLabels.data());

    std::vector<double> xs;
    for (std::size_t i = 0; i < values.size(); ++i) xs.push_back(static_cast<double>(i));
    ImPlotSpec spec;
    spec.FillColor = kSeriesPlay;
    spec.LineColor = kSurface;  // 隣の棒との間に、背景色のすき間を作る
    spec.LineWeight = ui::px(2.0f);
    ImPlot::PlotBars("##bars", xs.data(), values.data(), static_cast<int>(values.size()), 0.7, spec);

    if (ImPlot::IsPlotHovered() && !values.empty()) {
        const long index = std::lround(ImPlot::GetPlotMousePos().x);
        if (index >= 0 && index < static_cast<long>(values.size())) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(tooltip(static_cast<std::size_t>(index)).c_str());
            ImGui::EndTooltip();
        }
    }
    ImPlot::EndPlot();
}

/// 自己ベスト TOP 3（ハイライト）。1〜3 位は金・銀・銅の色の順位で示す
void drawTopResults(const std::vector<const SavedResult*>& top, GameMode mode, ImVec2 size) {
    ImGui::BeginChild("top", size, ImGuiChildFlags_Borders);
    ImGui::Text("自己ベスト TOP 3（%s）", scoreName(mode));
    ImGui::TextColored(kInkMuted, "総数 %d 以上のプレイから選びます", kMinTotalForBest);
    ImGui::Separator();
    if (top.empty()) ImGui::TextColored(kInkMuted, "まだありません");
    for (std::size_t i = 0; i < top.size(); ++i) {
        const SavedResult& saved = *top[i];
        const ResultScore score = scoreOf(saved.result);
        const ImVec4 color = ui::rankColor(static_cast<int>(i) + 1);
        ImGui::TextColored(color, "%zu 位", i + 1);
        ImGui::SameLine();
        ImGui::PushFont(nullptr, 26.0f);
        ImGui::TextColored(color, "%s", percentText(score).c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Text("%d / %d", score.count, score.total);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(kInkSecondary, "%s ／ %s ／ %s ／ %s", shortTime(saved.playedAt).c_str(),
                           saved.result.characterName.c_str(), saved.result.stageName.c_str(),
                           difficultyLabel(saved.result).c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

/// 絞り込んだ結果の表（新しい順）。自己ベストを更新したプレイに印を付ける
void drawResultTable(const std::vector<const SavedResult*>& results, GameMode mode) {
    const std::vector<std::size_t> updates = personalBestUpdates(results);
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("results", 7, kFlags, ImVec2{0.0f, ui::px(260.0f)})) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("日時");
    ImGui::TableSetupColumn(scoreName(mode));
    ImGui::TableSetupColumn("内訳");
    ImGui::TableSetupColumn("キャラ");
    ImGui::TableSetupColumn("ステージ");
    ImGui::TableSetupColumn(mode == GameMode::SkillDodge ? "敵" : "的（賢さ・大きさ・速さ）");
    ImGui::TableSetupColumn("");
    ImGui::TableHeadersRow();
    for (std::size_t n = results.size(); n-- > 0;) {
        const SavedResult& saved = *results[n];
        const ResultScore score = scoreOf(saved.result);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(saved.playedAt.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(percentText(score).c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d / %d", score.count, score.total);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(saved.result.characterName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(saved.result.stageName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(difficultyLabel(saved.result).c_str());
        ImGui::TableNextColumn();
        if (std::find(updates.begin(), updates.end(), n) != updates.end())
            ImGui::TextColored(kSeriesBest, "◆ 自己ベストを更新");
    }
    ImGui::EndTable();
}

}  // namespace

void applyPlotStyle() {
    ImPlotStyle& style = ImPlot::GetStyle();
    style.Colors[ImPlotCol_PlotBg] = kSurface;
    style.Colors[ImPlotCol_FrameBg] = kSurface;
    style.Colors[ImPlotCol_PlotBorder] = kAxis;
    style.Colors[ImPlotCol_AxisGrid] = kGrid;
    style.Colors[ImPlotCol_AxisTick] = kAxis;
    style.Colors[ImPlotCol_AxisText] = kInkSecondary;
    style.Colors[ImPlotCol_LegendBg] = ImVec4{0, 0, 0, 0};
    style.Colors[ImPlotCol_LegendBorder] = ImVec4{0, 0, 0, 0};
    style.Colors[ImPlotCol_LegendText] = kInkSecondary;
}

void StatsDashboard::drawMode(StatsFilter& filter, const ResultStore& store, const GameData& data) {
    const int today = localNow().date;
    drawFilters(filter, data);
    const std::vector<const SavedResult*> results = filterResults(store.all(filter.mode), filter, today);
    if (results.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kInkMuted, store.all(filter.mode).empty()
                                          ? "まだ記録がありません（制限時間まで遊ぶと記録されます）"
                                          : "この条件に合う記録がありません（絞り込みを変えてください）");
        return;
    }

    const StatsSummary summary = summarize(results, today);
    drawTiles(summary, filter.mode);
    ImGui::Spacing();

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float fullWidth = ImGui::GetContentRegionAvail().x;
    const float chartHeight = ui::px(300.0f);

    // 1 段目: 成績の推移（広く）と、日ごとのプレイ回数
    drawTrendPlot(results, filter.mode, ImVec2{fullWidth * 0.64f, chartHeight});
    ImGui::SameLine();
    const std::vector<DayStat> days = dailyStats(results);
    std::vector<std::string> dayLabels;
    std::vector<double> dayPlays;
    double maxPlays = 1.0;
    for (const DayStat& day : days) {
        dayLabels.push_back(shortDate(day.date));
        dayPlays.push_back(day.plays);
        maxPlays = std::max(maxPlays, static_cast<double>(day.plays));
    }
    drawBarPlot("日ごとのプレイ回数", dayLabels, dayPlays, std::ceil(maxPlays * 1.15), "%.0f",
                ImVec2{ImGui::GetContentRegionAvail().x, chartHeight}, [&](std::size_t i) {
                    return std::format("{}  {} 回\n{}: {}（{} / {}）", shortDate(days[i].date), days[i].plays,
                                       scoreName(filter.mode), percentText(days[i].score), days[i].score.count,
                                       days[i].score.total);
                });

    // 2 段目: 賢さごとの成績、キーごとのヒット率（スキルショット）、自己ベスト TOP 3
    const int columns = filter.mode == GameMode::SkillShot ? 3 : 2;
    const float columnWidth = (fullWidth - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const std::vector<LevelStat> levels = levelStats(results);
    std::vector<std::string> levelLabels;
    std::vector<double> levelRates;
    for (const LevelStat& level : levels) {
        levelLabels.push_back(
            std::format("賢さ {}", level.level));  // 名前はツールチップで出す（目盛りが詰まらないように）
        levelRates.push_back(level.score.rate() * 100.0);
    }
    const std::string levelTitle =
        std::format("{}の賢さごとの{}", filter.mode == GameMode::SkillDodge ? "敵" : "的", scoreName(filter.mode));
    drawBarPlot(levelTitle.c_str(), levelLabels, levelRates, 100.0, "%.0f%%", ImVec2{columnWidth, chartHeight},
                [&](std::size_t i) {
                    return std::format("賢さ {}（{}）  {} 回\n{}: {}（{} / {}）", levels[i].level, levels[i].name,
                                       levels[i].plays, scoreName(filter.mode), percentText(levels[i].score),
                                       levels[i].score.count, levels[i].score.total);
                });

    if (filter.mode == GameMode::SkillShot) {
        ImGui::SameLine();
        const std::array<HitCount, kSkillSlotCount> slots = hitsBySlot(results);
        std::vector<std::string> slotLabels;
        std::vector<double> slotRates;
        std::vector<HitCount> slotCounts;
        for (const SkillSlot slot : kAllSkillSlots) {
            const HitCount& count = slots[static_cast<std::size_t>(index(slot))];
            if (count.casts <= 0) continue;
            slotLabels.emplace_back(toString(slot));
            slotRates.push_back(count.rate() * 100.0);
            slotCounts.push_back(count);
        }
        drawBarPlot("キーごとのヒット率", slotLabels, slotRates, 100.0, "%.0f%%", ImVec2{columnWidth, chartHeight},
                    [&](std::size_t i) {
                        return std::format("{}  {:.1f}%（{} / {}）", slotLabels[i], slotCounts[i].rate() * 100.0f,
                                           slotCounts[i].hits, slotCounts[i].casts);
                    });
    }
    ImGui::SameLine();
    drawTopResults(topResults(results, 3), filter.mode, ImVec2{ImGui::GetContentRegionAvail().x, chartHeight});

    // 表（グラフを文字でも確かめられるように）
    if (ImGui::CollapsingHeader(std::format("この条件の記録の一覧（{} 件・新しい順）", results.size()).c_str())) {
        drawResultTable(results, filter.mode);
    }
}

bool StatsDashboard::draw(const ResultStore& store, const GameData& data) {
    bool close = false;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x * 0.96f, viewport->Size.y * 0.94f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.98f);
    ImGui::Begin("統計", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings);
    if (ImGui::Button("閉じる", ui::px(120.0f, 32.0f))) close = true;
    ImGui::SameLine();
    ImGui::TextColored(
        kInkMuted, "保存している記録（1 日 %zu 件まで・%d 日分）から集計します。グラフにカーソルを乗せると詳しく出ます",
        kMaxResultsPerDay, kResultKeepDays);
    if (ImGui::BeginTabBar("statsModes")) {
        const auto tabFlags = [&](GameMode mode) {
            return pendingMode_ == mode ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        };
        if (ImGui::BeginTabItem("スキルドッジ", nullptr, tabFlags(GameMode::SkillDodge))) {
            drawMode(dodgeFilter_, store, data);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("スキルショット", nullptr, tabFlags(GameMode::SkillShot))) {
            drawMode(shotFilter_, store, data);
            ImGui::EndTabItem();
        }
        pendingMode_.reset();
        ImGui::EndTabBar();
    }
    ImGui::End();
    return close;
}

}  // namespace moba
