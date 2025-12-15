#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

namespace
{
constexpr auto kManualBpmParamId = "manualBpm";
constexpr auto kInternalPlayParamId = "internalPlay";

const juce::Colour kBackgroundTop { 0xff0b1323 };
const juce::Colour kBackgroundBottom { 0xff0b1323 };
const juce::Colour kAccentBlue { 0xff32b7ff };
const juce::Colour kAccentBlue2 { 0xff2da5ff };
const juce::Colour kTextMuted { 0xff9aa8bd };
const juce::Colour kInnerRing { 0x442c3649 };

static juce::Path makeGearPath()
{
  juce::Path p;
  p.addEllipse(9.0f, 9.0f, 6.0f, 6.0f);
  p.addEllipse(5.5f, 5.5f, 13.0f, 13.0f);

  juce::Path teeth;
  teeth.addRectangle(11.3f, 0.4f, 1.4f, 4.0f);
  for (int i = 0; i < 8; ++i)
    p.addPath(teeth, juce::AffineTransform::rotation(juce::MathConstants<float>::twoPi * (float) i / 8.0f, 12.0f, 12.0f));

  p.setUsingNonZeroWinding(false);
  return p;
}

static juce::Path makeDocPath()
{
  juce::Path p;
  p.addRoundedRectangle(5.0f, 3.0f, 14.0f, 18.0f, 2.5f);
  p.addRectangle(7.5f, 8.0f, 9.0f, 1.3f);
  p.addRectangle(7.5f, 11.0f, 9.0f, 1.3f);
  p.addRectangle(7.5f, 14.0f, 6.0f, 1.3f);
  p.setUsingNonZeroWinding(false);
  return p;
}

static juce::Path makePlayPath()
{
  juce::Path p;
  p.addTriangle({ 9.0f, 7.0f }, { 18.0f, 12.0f }, { 9.0f, 17.0f });
  return p;
}

static juce::Path makePausePath()
{
  juce::Path p;
  p.addRoundedRectangle(9.0f, 7.0f, 3.3f, 10.0f, 1.0f);
  p.addRoundedRectangle(14.0f, 7.0f, 3.3f, 10.0f, 1.0f);
  return p;
}

static float clamp01(float v)
{
  return juce::jlimit(0.0f, 1.0f, v);
}

static float pulseFromBeatPhase(double beatPhase)
{
  const auto phase = clamp01(static_cast<float>(beatPhase));
  const auto decay = std::exp(-6.0f * phase);
  return clamp01(decay);
}

class IconButton final : public juce::Button
{
public:
  explicit IconButton(juce::Path iconPath)
      : juce::Button(""), icon(std::move(iconPath))
  {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool shouldDrawHover, bool isDown) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto bg = juce::Colours::white.withAlpha(isDown ? 0.10f : shouldDrawHover ? 0.07f : 0.0f);
    if (bg.getAlpha() > 0.0f)
    {
      g.setColour(bg);
      g.fillRoundedRectangle(bounds, 6.0f);
    }

    auto iconBounds = bounds.reduced(bounds.getWidth() * 0.18f);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillPath(icon, icon.getTransformToScaleToFit(iconBounds, true));
  }

private:
  juce::Path icon;
};

class StepButton final : public juce::Button
{
public:
  explicit StepButton(juce::String labelText)
      : juce::Button(""), label(std::move(labelText))
  {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool shouldDrawHover, bool isDown) override
  {
    auto bounds = getLocalBounds().toFloat();

    const auto bg = juce::Colours::white.withAlpha(isDown ? 0.12f : shouldDrawHover ? 0.08f : 0.02f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.35f);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(bounds.getHeight() * 0.55f, juce::Font::plain));
    g.drawText(label, getLocalBounds(), juce::Justification::centred);
  }

private:
  juce::String label;
};

class PlayPauseButton final : public juce::Button
{
public:
  PlayPauseButton()
      : juce::Button("")
  {
    setClickingTogglesState(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool shouldDrawHover, bool isDown) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto circle = bounds.withSizeKeepingCentre(size, size);

    const auto base = kAccentBlue;
    const auto fill = base.brighter((shouldDrawHover ? 0.08f : 0.0f) + (isDown ? 0.05f : 0.0f));

    g.setColour(fill);
    g.fillEllipse(circle);

    g.setColour(juce::Colours::black.withAlpha(0.92f));

    auto iconBounds = circle.reduced(circle.getWidth() * 0.30f);
    const auto icon = getToggleState() ? makePausePath() : makePlayPath();
    g.fillPath(icon, icon.getTransformToScaleToFit(iconBounds, true));
  }
};

class BpmReadout final : public juce::Component
{
public:
  void setBpm(double bpmToShow)
  {
    bpm = bpmToShow;
    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds();

    const auto bpmInt = static_cast<int>(std::round(bpm));
    const auto numberArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.68f));
    const auto labelArea = bounds;

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::Font(numberArea.getHeight() * 0.70f, juce::Font::bold));
    g.drawText(juce::String(bpmInt), numberArea, juce::Justification::centred);

    g.setColour(kTextMuted.withAlpha(0.85f));
    g.setFont(juce::Font(labelArea.getHeight() * 0.60f, juce::Font::plain));
    g.drawText("BPM", labelArea, juce::Justification::centred);
  }

private:
  double bpm = 120.0;
};

} // namespace

class VizBeatsAudioProcessorEditor::PulseVisualizer final : public juce::Component
{
public:
  void setPulse(float newPulse)
  {
    const float target = clamp01(newPulse);
    constexpr float smoothing = 0.10f;
    smoothedPulse = smoothedPulse * (1.0f - smoothing) + target * smoothing;
  }

  void setRunning(bool shouldRun) { running = shouldRun; }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();

    const auto maxRadius = size * 0.60f;
    const auto minRadius = size * 0.10f;

    const auto decay = (running ? smoothedPulse : 0.0f);
    const auto radius = minRadius + (maxRadius - minRadius) * decay;

    const auto alpha = 0.14f + 0.82f * decay;
    const auto stroke = juce::jlimit(1.4f, 9.5f, maxRadius * (0.010f + 0.024f * decay));

    const auto ringBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);

    g.setColour(kAccentBlue.withAlpha(alpha * 0.42f));
    g.fillEllipse(ringBounds);

    g.setColour(kAccentBlue.withAlpha(alpha * 0.78f));
    g.drawEllipse(ringBounds, stroke * 1.05f);

    // soft glow behind the core
    const auto glowRadius = size * 0.11f;
    auto glowBounds = juce::Rectangle<float>(glowRadius * 2.0f, glowRadius * 2.0f).withCentre(centre);
    juce::ColourGradient glow(kAccentBlue.withAlpha(0.28f * decay), centre, kAccentBlue.withAlpha(0.0f), glowBounds.getBottomRight(), true);
    g.setGradientFill(glow);
    g.fillEllipse(glowBounds);

    const auto coreRadius = size * 0.085f;
    const auto coreBounds = juce::Rectangle<float>(coreRadius * 2.0f, coreRadius * 2.0f).withCentre(centre);

    g.setColour(kAccentBlue2.withAlpha(0.97f));
    g.drawEllipse(coreBounds, stroke * 0.65f);

    const auto dotRadius = juce::jmax(2.6f, size * 0.011f);
    g.fillEllipse(juce::Rectangle<float>(dotRadius * 2.0f, dotRadius * 2.0f).withCentre(centre));
  }

private:
  float smoothedPulse = 1.0f;
  float pulse = 0.0f;
  bool running = false;
};

class VizBeatsAudioProcessorEditor::TransportBar final : public juce::Component
{
public:
  explicit TransportBar(VizBeatsAudioProcessor& p)
      : processor(p),
        gearButton(makeGearPath()),
        docButton(makeDocPath()),
        minusButton("-"),
        plusButton("+")
  {
    addAndMakeVisible(gearButton);
    addAndMakeVisible(docButton);
    addAndMakeVisible(minusButton);
    addAndMakeVisible(plusButton);
    addAndMakeVisible(bpmReadout);
    addAndMakeVisible(playPauseButton);

    gearButton.setTooltip("About");
    docButton.setTooltip("Open demo link");
    minusButton.setTooltip("Decrease manual BPM");
    plusButton.setTooltip("Increase manual BPM");
    playPauseButton.setTooltip("Internal preview play/pause");

    gearButton.onClick = [this]
    {
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon,
          "VizBeats",
          "Visual metronome plugin.\n\nSyncs to host tempo/transport when available.\nManual BPM is used for internal preview only.");
    };

    docButton.onClick = []
    {
      juce::URL("https://vizbeats-440052928226.us-west1.run.app/").launchInDefaultBrowser();
    };

    minusButton.onClick = [this] { nudgeManualBpm(-1.0f); };
    plusButton.onClick = [this] { nudgeManualBpm(1.0f); };

    playPauseButton.onClick = [this]
    {
      if (hostPlaying)
        return;

      if (auto* v = processor.apvts.getParameter(kInternalPlayParamId))
        v->setValueNotifyingHost(playPauseButton.getToggleState() ? 1.0f : 0.0f);
    };
  }

  void setHostPlaying(bool isHostPlaying)
  {
    hostPlaying = isHostPlaying;
    playPauseButton.setEnabled(!hostPlaying);
    repaint();
  }

  void setPlayState(bool isInternalPlaying)
  {
    if (hostPlaying)
    {
      playPauseButton.setToggleState(true, juce::dontSendNotification);
      return;
    }

    playPauseButton.setToggleState(isInternalPlaying, juce::dontSendNotification);
  }

  void setBpm(double bpm)
  {
    bpmReadout.setBpm(bpm);
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto radius = bounds.getHeight() * 0.50f;

    juce::ColourGradient grad(
        juce::Colour(0xff1e2b3e).withAlpha(0.92f),
        bounds.getTopLeft(),
        juce::Colour(0xff172336).withAlpha(0.92f),
        bounds.getBottomRight(),
        false);

    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(bounds.reduced(0.8f), radius, 1.0f);

    auto playArea = getPlayButtonArea().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(playArea.getX() - 14.0f, bounds.getY() + 16.0f, playArea.getX() - 14.0f, bounds.getBottom() - 16.0f, 1.0f);
  }

  void resized() override
  {
    auto bounds = getLocalBounds();

    auto right = bounds.removeFromRight(bounds.getHeight());
    playPauseButton.setBounds(right.reduced(10));

    bounds.removeFromRight(18);

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getHeight() * 1.35f));
    const auto iconSize = static_cast<int>(bounds.getHeight() * 0.44f);
    const auto iconY = (getHeight() - iconSize) / 2;

    gearButton.setBounds(left.removeFromLeft(iconSize).withY(iconY).withHeight(iconSize));
    left.removeFromLeft(12);
    docButton.setBounds(left.removeFromLeft(iconSize).withY(iconY).withHeight(iconSize));

    auto centre = bounds.reduced(10, 14);

    const auto stepW = static_cast<int>(centre.getWidth() * 0.18f);
    minusButton.setBounds(centre.removeFromLeft(stepW).reduced(0, 8));
    centre.removeFromLeft(10);
    plusButton.setBounds(centre.removeFromRight(stepW).reduced(0, 8));

    bpmReadout.setBounds(centre.reduced(6, 0));
  }

private:
  void nudgeManualBpm(float delta)
  {
    auto* raw = processor.apvts.getRawParameterValue(kManualBpmParamId);
    if (raw == nullptr)
      return;

    auto* param = processor.apvts.getParameter(kManualBpmParamId);
    if (param == nullptr)
      return;

    const auto current = raw->load();
    const auto next = juce::jlimit(30.0f, 300.0f, current + delta);

    const auto range = param->getNormalisableRange();
    const auto normalised = range.convertTo0to1(next);
    param->setValueNotifyingHost(normalised);
  }

  juce::Rectangle<int> getPlayButtonArea() const
  {
    auto bounds = getLocalBounds();
    return bounds.removeFromRight(bounds.getHeight()).reduced(10);
  }

  VizBeatsAudioProcessor& processor;

  IconButton gearButton;
  IconButton docButton;
  StepButton minusButton;
  StepButton plusButton;
  BpmReadout bpmReadout;
  PlayPauseButton playPauseButton;

  bool hostPlaying = false;
};

VizBeatsAudioProcessorEditor::VizBeatsAudioProcessorEditor(VizBeatsAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
  setOpaque(true);

  visualizer = std::make_unique<PulseVisualizer>();
  transportBar = std::make_unique<TransportBar>(processor);

  addAndMakeVisible(*visualizer);
  addAndMakeVisible(*transportBar);

  setSize(960, 540);
  startTimerHz(60);
}

VizBeatsAudioProcessorEditor::~VizBeatsAudioProcessorEditor()
{
  stopTimer();
}

void VizBeatsAudioProcessorEditor::paint(juce::Graphics& g)
{
  auto bounds = getLocalBounds().toFloat();
  g.setColour(kBackgroundTop);
  g.fillRect(bounds);

  if (transportBar != nullptr)
  {
    const juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 18, { 0, 8 });
    shadow.drawForRectangle(g, transportBar->getBounds());
  }
}

void VizBeatsAudioProcessorEditor::resized()
{
  auto bounds = getLocalBounds();

  const auto barHeight = 92;
  const auto barWidth = juce::jmin(700, bounds.getWidth() - 28);
  const auto barX = (bounds.getWidth() - barWidth) / 2;
  const auto barY = bounds.getBottom() - barHeight - 18;

  if (transportBar != nullptr)
    transportBar->setBounds(barX, barY, barWidth, barHeight);

  auto vizBounds = bounds.withTrimmedTop(24).withTrimmedBottom(barHeight + 42).reduced(28);
  if (visualizer != nullptr)
    visualizer->setBounds(vizBounds);
}

void VizBeatsAudioProcessorEditor::timerCallback()
{
  processor.refreshHostInfo();

  const auto hostInfo = processor.getHostInfo();
  const auto manualBpm = static_cast<double>(processor.apvts.getRawParameterValue(kManualBpmParamId)->load());
  const auto internalPlay = processor.apvts.getRawParameterValue(kInternalPlayParamId)->load() > 0.5f;

  if (internalPlay && !lastInternalPlayState)
    internalStartTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

  lastInternalPlayState = internalPlay;

  const auto hostPlaying = hostInfo.isPlaying;
  const auto effectiveBpm = hostInfo.hasBpm ? hostInfo.bpm : manualBpm;

  transportBar->setHostPlaying(hostPlaying);
  transportBar->setPlayState(internalPlay);
  transportBar->setBpm(effectiveBpm);

  bool isRunning = false;
  double beatPhase = 0.0;

  if (hostPlaying && hostInfo.hasPpqPosition)
  {
    isRunning = true;
    const auto ppq = hostInfo.ppqPosition;
    beatPhase = ppq - std::floor(ppq);
    if (beatPhase < 0.0)
      beatPhase += 1.0;
  }
  else if (internalPlay)
  {
    isRunning = true;
    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsedSeconds = juce::jmax(0.0, nowSeconds - internalStartTimeSeconds);
    const auto beats = elapsedSeconds * (manualBpm / 60.0);
    beatPhase = beats - std::floor(beats);
  }

  bool beatWrapped = false;
  if (isRunning)
  {
    beatWrapped = lastUiRunning && (beatPhase < lastBeatPhaseUi);
    lastBeatPhaseUi = static_cast<float>(beatPhase);
  }
  else
  {
    lastBeatPhaseUi = 0.0f;
  }
  lastUiRunning = isRunning;

  visualizer->setRunning(isRunning);
  visualizer->setPulse(isRunning ? (beatWrapped ? 1.0f : pulseFromBeatPhase(beatPhase)) : 0.0f);
  visualizer->repaint();
  transportBar->repaint();
}
