﻿#include "diagnostics/performance_stats.h"

#include "system.h"
#include "world.h"
#include "halley/net/session/network_session.h"
#include "halley/core/api/core_api.h"
#include "halley/core/api/halley_api.h"
#include "halley/core/graphics/painter.h"
#include "halley/core/resources/resources.h"
#include "halley/net/connection/ack_unreliable_connection_stats.h"
#include "halley/support/logger.h"
#include "halley/support/profiler.h"
#include "halley/time/halleytime.h"
#include "halley/time/stopwatch.h"

using namespace Halley;

PerformanceStatsView::PerformanceStatsView(Resources& resources, const HalleyAPI& api)
	: StatsView(resources, api)
	, boxBg(Sprite().setImage(resources, "halley/box_2px_outline.png"))
	, whitebox(Sprite().setImage(resources, "whitebox.png"))
	, audioTime(60)
	, vsyncTime(60)
	, totalFrameTime(60)
	, updateTime(60)
	, renderTime(60)
{
	api.core->addProfilerCallback(this);
	
	headerText = TextRenderer(resources.get<Font>("Ubuntu Bold"), "", 16, Colour(1, 1, 1), 1.0f, Colour(0.1f, 0.1f, 0.1f));
	fpsLabel = TextRenderer(resources.get<Font>("Ubuntu Bold"), "", 15, Colour(1, 1, 1), 1.0f, Colour(0.1f, 0.1f, 0.1f)).setOffset(Vector2f(0.5f, 0.5f));
	graphLabel = TextRenderer(resources.get<Font>("Ubuntu Bold"), "", 15, Colour(1, 1, 1), 1.0f, Colour(0.1f, 0.1f, 0.1f)).setAlignment(0.5f);
	connLabel = TextRenderer(resources.get<Font>("Ubuntu Bold"), "", 15, Colour(1, 1, 1), 1.0f, Colour(0.1f, 0.1f, 0.1f));

	for (size_t i = 0; i < 3; ++i) {
		systemLabels.push_back(headerText.clone());
	}

	constexpr size_t frameDataCapacity = 300;
	frameData.resize(frameDataCapacity);
	lastFrameData = frameDataCapacity - 1;
}

PerformanceStatsView::~PerformanceStatsView()
{
	api.core->removeProfilerCallback(this);
}

void PerformanceStatsView::update()
{
	//capturing = !active;
	StatsView::update();
}

void PerformanceStatsView::paint(Painter& painter)
{
	ProfilerEvent event(ProfilerEventType::StatsView);
	painter.setLogging(false);

	if (active) {
		whitebox.clone().setPosition(Vector2f(0, 0)).scaleTo(Vector2f(painter.getViewPort().getSize())).setColour(Colour4f(0, 0, 0, 0.5f)).draw(painter);

		const auto rect = Rect4f(painter.getViewPort());

		drawHeader(painter, false);
		drawTimeline(painter, Rect4f(20, 80,	rect.getWidth() - 40, 100));

		if (page == 0) {
			drawTimeGraph(painter, Rect4f(20, 200, rect.getWidth() - 40, rect.getHeight() - 220));
		} else if (page == 1) {
			drawTopSystems(painter, Rect4f(20, 200, rect.getWidth() - 40, rect.getHeight() - 220));
		} else if (page == 2) {
			drawNetworkStats(painter, Rect4f(20, 200, rect.getWidth() - 40, rect.getHeight() - 220));
		}
	} else {
		drawHeader(painter, true);
	}
	
	painter.setLogging(true);
}

void PerformanceStatsView::onProfileData(std::shared_ptr<ProfilerData> data)
{
	vsyncTime.pushValue(data->getElapsedTime(ProfilerEventType::CoreVSync).count());
	updateTime.pushValue(data->getElapsedTime(ProfilerEventType::CoreVariableUpdate).count() + data->getElapsedTime(ProfilerEventType::CoreFixedUpdate).count());
	renderTime.pushValue(data->getElapsedTime(ProfilerEventType::CoreRender).count());
	totalFrameTime.pushValue(data->getTotalElapsedTime().count() - vsyncTime.getLatest());
	audioTime.pushValue(api.audio->getLastTimeElapsed());

	auto getTime = [&](TimeLine timeline) -> int
	{
		const auto ns = getTimeNs(timeline, *data);
		return static_cast<int>((ns + 500) / 1000);
	};

	lastFrameData = (lastFrameData + 1) % frameData.size();
	auto& curFrameData = frameData[lastFrameData];
	curFrameData.fixedTime = getTime(TimeLine::FixedUpdate);
	curFrameData.variableTime = getTime(TimeLine::VariableUpdate);
	curFrameData.renderTime = getTime(TimeLine::Render) - static_cast<int>((vsyncTime.getLatest() + 500) / 1000);

	for (const auto& e: data->getEvents()) {
		if (e.type == ProfilerEventType::WorldSystemUpdate || e.type == ProfilerEventType::WorldSystemRender) {
			eventHistory[e.name].update(e.type, (e.endTime - e.startTime).count());
		}
	}

	if (capturing) {
		lastProfileData = std::move(data);
	}
}

void PerformanceStatsView::setNetworkStats(NetworkSession& session)
{
	networkStats = &session.getService();
	networkSession = &session;
}

int PerformanceStatsView::getNumPages() const
{
	return networkStats ? 3 : 2;
}

int PerformanceStatsView::getPage() const
{
	return page;
}

void PerformanceStatsView::setPage(int page)
{
	this->page = page;
}

PerformanceStatsView::EventHistoryData::EventHistoryData()
	: average(120)
{
}

void PerformanceStatsView::EventHistoryData::update(ProfilerEventType type, int64_t value)
{
	this->type = type;
	const bool reset = average.pushValue(value);
	if (reset) {
		lastHighest = highest;
		lastLowest = lowest;
		highest = 0;
		lowest = std::numeric_limits<int64_t>::max();
	}
	highest = std::max(highest, value);
	lowest = std::min(lowest, value);
	highestEver = std::max(highestEver, value);
	lowestEver = std::min(lowestEver, value);
}

int64_t PerformanceStatsView::EventHistoryData::getAverage() const
{
	return average.getAverage();
}

int64_t PerformanceStatsView::EventHistoryData::getHighest() const
{
	return lastHighest;
}

int64_t PerformanceStatsView::EventHistoryData::getLowest() const
{
	return lastLowest;
}

int64_t PerformanceStatsView::EventHistoryData::getHighestEver() const
{
	return highestEver;
}

int64_t PerformanceStatsView::EventHistoryData::getLowestEver() const
{
	return lowestEver;
}

ProfilerEventType PerformanceStatsView::EventHistoryData::getType() const
{
	return type;
}

void PerformanceStatsView::drawHeader(Painter& painter, bool simple)
{
	const auto frameAvgTime = totalFrameTime.getAverage();
	const auto updateAvgTime = updateTime.getAverage();
	const auto renderAvgTime = renderTime.getAverage();
	const auto vsyncAvgTime = vsyncTime.getAverage();
	const auto audioAvgTime = audioTime.getAverage();
	const int curFPS = static_cast<int>(lround(1'000'000'000.0 / (frameAvgTime + vsyncAvgTime)));
	const int maxFPS = static_cast<int>(lround(1'000'000'000.0 / std::max(updateAvgTime, renderAvgTime)));

	const auto updateCol = Colour4f(0.69f, 0.75f, 0.98f);
	const auto renderCol = Colour4f(0.98f, 0.69f, 0.69f);

	ColourStringBuilder strBuilder;
	strBuilder.append(toString(curFPS, 10, 3, ' '), curFPS < maxFPS ? std::optional<Colour4f>() : Colour4f(1, 0, 0));
	strBuilder.append(" FPS | ");
	strBuilder.append(toString(maxFPS, 10, 4, ' '), updateAvgTime > renderAvgTime ? updateCol : renderCol);
	strBuilder.append(" FPS | ");
	strBuilder.append(formatTime(updateAvgTime), updateCol);
	strBuilder.append(" ms / ");
	strBuilder.append(formatTime(renderAvgTime), renderCol);
	strBuilder.append(" ms | ");
	strBuilder.append(toString(painter.getPrevDrawCalls()));
	strBuilder.append(" calls | ");
	strBuilder.append(toString(painter.getPrevTriangles()));
	strBuilder.append(" tris");

	if (networkStats) {
		strBuilder.append(" | up: ");
		strBuilder.append(toString(networkStats->getSentDataPerSecond() / 1000.0, 3) + " kBps");
		strBuilder.append(" (");
		strBuilder.append(toString(networkStats->getSentPacketsPerSecond()));
		strBuilder.append(") | down: ");
		strBuilder.append(toString(networkStats->getReceivedDataPerSecond() / 1000.0, 3) + " kBps");
		strBuilder.append(" (");
		strBuilder.append(toString(networkStats->getReceivedPacketsPerSecond()));
		strBuilder.append(")");
	}

	if (!simple) {
		const auto audioSpec = api.audio->getAudioSpec();
		if (audioSpec) {
			const int64_t totalTimePerBuffer = int64_t(audioSpec->bufferSize) * 1'000'000'000 / int64_t(audioSpec->sampleRate);
			const auto percent = (audioAvgTime * 100.0f) / static_cast<float>(totalTimePerBuffer);
			strBuilder.append(" | ");
			strBuilder.append(formatTime(audioAvgTime));
			strBuilder.append(" ms audio (");
			strBuilder.append(toString(percent, 1));
			strBuilder.append("%)");
		}
	}

	if (simple) {
		const auto pos = Vector2f(painter.getViewPort().getTopRight()) + Vector2f(-5, 5);
		headerText.setPosition(pos).setOffset(Vector2f(1, 0)).setOutline(2.0f);
	} else {
		headerText.setPosition(Vector2f(10, 10)).setOffset(Vector2f()).setOutline(1.0f);
	}

	auto [str, colours] = strBuilder.moveResults();
	
	headerText
		.setText(str)
		.setColourOverride(colours)
		.draw(painter);
}

void PerformanceStatsView::drawTimeline(Painter& painter, Rect4f rect)
{
	const Vector2f displaySize = rect.getSize() - Vector2f(40, 0);
	const auto pos = rect.getTopLeft();
	const float maxFPS = 30.0f;
	const float scale = maxFPS / 1'000'000.0f * displaySize.y;

	const Vector2f boxPos = pos + Vector2f(20, 0);
	boxBg
		.clone()
		.setPosition(boxPos - Vector2f(2, 2))
		.scaleTo(displaySize + Vector2f(4, 4))
		.draw(painter);

	auto drawBar = [&](Vector2f pos, const String& label)
	{
		whitebox
			.clone()
			.setPosition(pos)
			.scaleTo(Vector2f(displaySize.x, 1))
			.setColour(Colour4f(0.75f))
			.draw(painter);
		fpsLabel.setPosition(pos - Vector2f(15.0f, 0.0f)).setText(label).draw(painter);
		fpsLabel.setPosition(pos + Vector2f(displaySize.x + 15.0f, 00.0f)).draw(painter);
	};

	drawBar(boxPos, "30");
	drawBar(boxPos + Vector2f(0, displaySize.y / 2), "60");
	drawBar(boxPos + Vector2f(0, 3 * displaySize.y / 4), "120");

	auto variableSprite = whitebox
		.clone()
		.setPivot(Vector2f(0, 1))
		.setColour(Colour4f(0.5f, 0.4f, 1.0f));

	auto renderSprite = whitebox
		.clone()
		.setPivot(Vector2f(0, 1))
		.setColour(Colour4f(1.0f, 0.2f, 0.2f));

	const size_t n = frameData.size();
	const float oneOverN = 1.0f / static_cast<float>(n);
	const auto& xPos = [&](size_t i) -> float
	{
		return static_cast<float>(i) * displaySize.x * oneOverN;
	};

	for (size_t i = 0; i < n; ++i) {
		const size_t index = (i + lastFrameData + 1) % n;

		const float x = xPos(i);
		const float w = xPos(i + 1) - x;
		const Vector2f p = boxPos + Vector2f(x, displaySize.y);
		Vector2f s1 = Vector2f(w, std::min(frameData[index].variableTime * scale, displaySize.y));
		Vector2f s2 = Vector2f(w, std::min(frameData[index].renderTime * scale, displaySize.y));
		if (s1.y > s2.y) {
			if (s1.y < 1) {
				s1.y = 1;
			}
			variableSprite.setPosition(p).setSize(s1).draw(painter);
		} else {
			if (s2.y < 1) {
				s2.y = 1;
			}
			renderSprite.setPosition(p).setSize(s2).draw(painter);
		}
	}
}

void PerformanceStatsView::drawTimeGraph(Painter& painter, Rect4f rect)
{
	if (!lastProfileData) {
		return;
	}

	const auto duration = std::chrono::duration<int64_t, std::nano>(16'999'999);
	const auto timeRange = Range<ProfilerData::TimePoint>(lastProfileData->getStartTime(), lastProfileData->getStartTime() + duration);
	const int64_t numMs = timeRange.getLength().count() / 1'000'000;

	boxBg
		.clone()
		.setPosition(rect.getTopLeft())
		.scaleTo(rect.getSize())
		.draw(painter);

	// Vertical dividers
	const float divPerNs = rect.getWidth() / static_cast<float>(timeRange.getLength().count());
	auto drawDivider = [&] (int64_t ns, Colour4f col, String label)
	{
		const float xPos = static_cast<float>(ns) * divPerNs;
		whitebox
			.clone()
			.setPosition(rect.getTopLeft() + Vector2f(xPos, 0))
			.scaleTo(Vector2f(1, rect.getHeight()))
			.setColour(col)
			.draw(painter);

		if (!label.isEmpty()) {
			graphLabel
				.setPosition(rect.getBottomLeft() + Vector2f(xPos, 0))
				.setColour(col)
				.setText(label)
				.draw(painter);
		}
	};

	// 1 ms dividers
	for (int64_t i = 1; i <= numMs; ++i) {
		drawDivider(i * 1'000'000, Colour4f(0.75f), toString(i) + " ms");
	}

	// Vsync divider
	drawDivider(16'666'666, Colour4f(1.0f, 0.75f, 0.0f), "vsync");

	drawTimeGraphThreads(painter, rect.shrink(2), timeRange);
}

void PerformanceStatsView::drawTimeGraphThreads(Painter& painter, Rect4f rect, Range<ProfilerData::TimePoint> timeRange)
{
	const auto& threads = lastProfileData->getThreads();

	int totalThreadDepth = 0;
	for (const auto& threadInfo: threads) {
		totalThreadDepth += threadInfo.maxDepth + 1;
	}

	const float threadSpacing = 20.0f;
	const float threadHeight = std::min(50.0f, std::floor((rect.getHeight() - (threads.size() - 1) * threadSpacing) / static_cast<float>(totalThreadDepth)));
	float heightSoFar = 0;
	for (const auto& threadInfo: threads) {
		const int depth = threadInfo.maxDepth + 1;
		drawTimeGraphThread(painter, Rect4f(rect.getLeft(), rect.getTop() + heightSoFar, rect.getWidth(), threadHeight * depth), threadInfo, timeRange);
		heightSoFar += static_cast<float>(depth) * threadHeight + threadSpacing;
	}
}

void PerformanceStatsView::drawTimeGraphThread(Painter& painter, Rect4f rect, const ProfilerData::ThreadInfo& threadInfo, Range<ProfilerData::TimePoint> timeRange)
{
	auto box = whitebox.clone();
	const auto frameStartTime = timeRange.start;
	const auto frameEndTime = timeRange.end;
	const auto frameLength = frameEndTime - frameStartTime;

	const auto origin = rect.getTopLeft();
	const auto lineHeight = std::floor(rect.getHeight() / static_cast<float>(threadInfo.maxDepth + 1));

	for (const auto& e: lastProfileData->getEvents()) {
		if (e.threadId == threadInfo.id) {
			const float relativeStart = static_cast<float>((e.startTime - frameStartTime).count()) / static_cast<float>(frameLength.count());
			const float relativeEnd = static_cast<float>((e.endTime - frameStartTime).count()) / static_cast<float>(frameLength.count());

			const float startPos = std::floor(relativeStart * rect.getWidth());
			const float endPos = std::floor(relativeEnd * rect.getWidth());
			const auto eventRect = origin + Rect4f(startPos, e.depth * lineHeight, std::max(endPos - startPos, 1.0f), lineHeight);

			const auto col = getEventColour(e.type);
			if (eventRect.getSize().x > 0.95f) {
				box
					.setColour(col.multiplyAlpha(0.5f))
					.setPosition(eventRect.getTopLeft() + Vector2f(1, 0))
					.scaleTo(eventRect.getSize() - Vector2f(1, 0))
					.draw(painter);
			}
			box
				.setColour(col)
				.scaleTo(Vector2f(1.0f, eventRect.getHeight()))
				.draw(painter);
		}
	}
}

Colour4f PerformanceStatsView::getEventColour(ProfilerEventType type) const
{
	switch (type) {
	case ProfilerEventType::CoreVSync:
		return Colour4f(0.8f, 0.8f, 0.1f);
	case ProfilerEventType::CoreUpdate:
	case ProfilerEventType::CoreFixedUpdate:
	case ProfilerEventType::CoreVariableUpdate:
	case ProfilerEventType::WorldSystemUpdate:
	case ProfilerEventType::WorldFixedUpdate:
	case ProfilerEventType::WorldVariableUpdate:
		return Colour4f(0.1f, 0.1f, 0.7f);
	case ProfilerEventType::CoreRender:
	case ProfilerEventType::WorldSystemRender:
	case ProfilerEventType::WorldRender:
		return Colour4f(0.7f, 0.1f, 0.1f);
	case ProfilerEventType::PainterDrawCall:
		return Colour4f(0.97f, 0.51f, 0.65f);
	case ProfilerEventType::CoreStartRender:
	case ProfilerEventType::PainterEndRender:
		return Colour4f(1.0f, 0.61f, 0.75f);
	case ProfilerEventType::PainterUpdateProjection:
		return Colour4f(1.0f, 0.71f, 0.85f);
	case ProfilerEventType::StatsView:
		return Colour4f(0.7f, 0.7f, 0.7f);
	case ProfilerEventType::AudioGenerateBuffer:
		return Colour4f(0.5f, 0.8f, 1.0f);
	default:
		return Colour4f(0.1f, 0.7f, 0.1f);
	}
}

Colour4f PerformanceStatsView::getNetworkStatsCol(const AckUnreliableConnectionStats::PacketStats& stats) const
{
	if (stats.size == 8 && stats.state != AckUnreliableConnectionStats::State::Unsent) {
		return Colour4f(0.5f, 0.5f, 0.5f);
	}

	switch (stats.state) {
	case AckUnreliableConnectionStats::State::Sent:
		return Colour4f(1, 1, 0, 1);
	case AckUnreliableConnectionStats::State::Acked:
		return Colour4f(0, 1, 0, 1);
	case AckUnreliableConnectionStats::State::Resent:
		return Colour4f(1, 0, 0, 1);
	case AckUnreliableConnectionStats::State::Received:
		return Colour4f(0, 0.2f, 1, 1);
	case AckUnreliableConnectionStats::State::Unsent:
	default:
		return Colour4f(0, 0, 0, 0);
	}
}

void PerformanceStatsView::drawTopSystems(Painter& painter, Rect4f rect)
{
	struct CurEventData {
		const String* name;
		ProfilerEventType type;
		int64_t avg;
		int64_t high;
		int64_t low;
		int64_t highEver;
		int64_t lowEver;
		Colour4f colour;

		bool operator< (const CurEventData& other) const
		{
			return avg > other.avg;
		}
	};

	const auto getTimeLabel = [&] (int64_t t) { return toString((t + 500) / 1000); };

	const auto drawBoxPlot = [&] (const CurEventData& system, Rect4f rect, float scale, Colour4f colour)
	{
		const float low = system.low * scale;
		const float lowEver = system.lowEver * scale;
		const float high = system.high * scale;
		const float highEver = system.highEver * scale;
		const float avg = system.avg * scale;

		const float everMargin = (rect.getHeight() - 1) / 2;

		const auto transpColour = colour.inverseMultiplyLuma(0.5f).withAlpha(0.7f);

		whitebox.clone()
	        .setPos(rect.getTopLeft() + Vector2f(std::floor(low), 0))
	        .scaleTo(Vector2f(std::floor(high) - std::floor(low), rect.getHeight()))
	        .setColour(transpColour)
	        .draw(painter);

		whitebox.clone()
			.setPos(rect.getTopLeft() + Vector2f(std::floor(lowEver), everMargin))
			.scaleTo(Vector2f(std::floor(low) - std::floor(lowEver), 1))
			.setColour(transpColour)
			.draw(painter);

		whitebox.clone()
			.setPos(rect.getTopLeft() + Vector2f(std::floor(high), everMargin))
			.scaleTo(Vector2f(std::floor(highEver) - std::floor(high), 1))
			.setColour(transpColour)
			.draw(painter);

		whitebox.clone()
			.setPos(rect.getTopLeft() + Vector2f(std::floor(avg) - 1.0f, 0))
			.scaleTo(Vector2f(2, rect.getHeight()))
			.setColour(colour.inverseMultiplyLuma(0.2f))
			.draw(painter);
	};

	std::array<float, 3> xPos = { 0, 350, 450 };
	float barDrawX = 390;
	const int64_t granularity = 500'000;
	int64_t maxTime = granularity;

	Vector<CurEventData> curEvents;
	curEvents.reserve(eventHistory.size());
	for (const auto& [k, v]: eventHistory) {
		const auto col = getEventColour(v.getType());
		curEvents.emplace_back(CurEventData{ &k, v.getType(), v.getAverage(), v.getHighest(), v.getLowest(), v.getHighestEver(), v.getLowestEver(), col });
		maxTime = std::max(maxTime, curEvents.back().high);
	}
	std::sort(curEvents.begin(), curEvents.end());

	const float maxTimeRounded = static_cast<float>(std::pow(2, std::ceil(std::log2(static_cast<float>(maxTime) / granularity))) * granularity);
	const float scale = (rect.getWidth() - barDrawX) / maxTimeRounded;

	const auto lineHeight = systemLabels[0].getLineHeight();
	const size_t nToShow = static_cast<size_t>(std::floor(rect.getHeight() / lineHeight));

	Vector<ColourStringBuilder> columns;
	columns.resize(systemLabels.size());

	columns[0].append("Name:\n");
	columns[1].append("Avg:\n");
	columns[2].append("Bounds:\n");

	// Vertical bars
	for (int64_t time = 0; time < static_cast<int64_t>(maxTimeRounded); time += granularity) {
		whitebox.clone()
			.setPos(rect.getTopLeft() + Vector2f(barDrawX + time * scale, lineHeight))
			.scaleTo(Vector2f(1, rect.getHeight() - lineHeight))
			.setColour(Colour4f(1, 1, 1, 0.25f))
			.draw(painter);
	}

	for (size_t i = 0; i < nToShow; ++i) {
		const auto& system = curEvents[i];
		columns[0].append(toString(i + 1) + ": ");
		columns[0].append(*system.name + "\n", system.colour.inverseMultiplyLuma(0.5f));
		columns[1].append(getTimeLabel(system.avg) + " us\n", system.colour.inverseMultiplyLuma(0.5f));

		const auto pos = rect.getTopLeft() + Vector2f(barDrawX, (i + 1) * lineHeight);
		drawBoxPlot(system, Rect4f(pos, Vector2f(rect.getRight(), pos.y + lineHeight)).shrink(1.0f), scale, system.colour);
	}

	for (size_t i = 1; i < systemLabels.size(); ++i) {
		systemLabels[i].setAlignment(1);
	}

	for (size_t i = 0; i < systemLabels.size(); ++i) {
		auto [str, cols] = columns[i].moveResults();
		systemLabels[i].setText(str).setColourOverride(cols);
		systemLabels[i].setPosition(rect.getTopLeft() + Vector2f(xPos[i], 0)).draw(painter);
	}
}

void PerformanceStatsView::drawNetworkStats(Painter& painter, Rect4f rect)
{
	if (!networkSession) {
		return;
	}
	const size_t nConnections = networkSession->getNumConnections();
	if (nConnections == 0) {
		return;
	}

	const float spacing = 20.0f;
	const float boxHeight = std::min(128.0f, (rect.getHeight() - (nConnections - 1) * spacing) / nConnections);

	auto box = whitebox.clone();

	for (size_t i = 0; i < nConnections; ++i) {
		const Rect4f totalArea = Rect4f(rect.getTopLeft() + Vector2f(0, i * (boxHeight + spacing)), rect.getWidth(), boxHeight);
		const Rect4f area = totalArea.grow(0, -20, 0, 0);

		connLabel
			.setPosition(totalArea.getTopLeft())
			.setText("Connection #" + toString(i + 1) + ": latency = " + toString(lroundl(networkSession->getLatency(i) * 1000)) + " ms.")
			.draw(painter);

		boxBg
			.clone()
			.setPosition(area.getTopLeft())
			.scaleTo(area.getSize())
			.draw(painter);

		const auto& connStats = networkSession->getConnectionStats(i);
		const auto& stats = connStats.getPacketStats();
		const auto start = connStats.getLineStart();
		const auto lineLen = connStats.getLineSize();
		const size_t nLines = (stats.size() + lineLen - 1) / lineLen;
		const float width = area.getWidth() / lineLen;
		const float height = area.getHeight() / nLines;

		for (size_t j = 0; j < stats.size(); ++j) {
			const size_t x = j % lineLen;
			const size_t y = j / lineLen;

			const auto& stat = stats[(j + start) % stats.size()];
			const auto eventRect = Rect4f(area.getTopLeft() + Vector2f(x * width, y * height), width, height);
			const auto border = Vector2f(4, 4);
			
			box
				.setColour(getNetworkStatsCol(stat))
				.setPosition(eventRect.getTopLeft() + border)
				.scaleTo(eventRect.getSize() - 2 * border)
				.draw(painter);
		}
	}
}

int64_t PerformanceStatsView::getTimeNs(TimeLine timeline, const ProfilerData& data)
{
	// Total time
	const auto totalTime = data.getTotalElapsedTime();

	// Render time
	const auto renderTime = data.getElapsedTime(ProfilerEventType::CoreRender);
	const auto vsyncTime = data.getElapsedTime(ProfilerEventType::CoreVSync);
	
	if (timeline == TimeLine::VariableUpdate) {
		return (totalTime - renderTime - vsyncTime).count();
	} else if (timeline == TimeLine::Render) {
		return renderTime.count();
	} else {
		return 0;
	}
}
