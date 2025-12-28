#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr auto kManualBpmParamId = "manualBpm";
constexpr auto kInternalPlayParamId = "internalPlay";
constexpr auto kVisualModeParamId = "visualMode";
constexpr auto kColorThemeParamId = "colorTheme";
constexpr auto kBeatsPerBarParamId = "beatsPerBar";
constexpr auto kSubdivisionsParamId = "subdivisions";
constexpr auto kSoundVolumeParamId = "soundVolume";
constexpr auto kPreviewSubdivisionsParamId = "previewSubdivisions";
}

VizBeatsAudioProcessor::VizBeatsAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VizBeatsAudioProcessor::createParameterLayout()
{
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID { kManualBpmParamId, 1 },
      "Manual BPM",
      juce::NormalisableRange<float>(30.0f, 300.0f, 1.0f),
      60.0f));

  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID { kInternalPlayParamId, 1 }, "Internal Play", false));

  // Visual mode: 0=Pulse, 1=Traffic, 2=Pendulum, 3=Bounce, 4=Ladder, 5=Pattern
  params.push_back(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID { kVisualModeParamId, 1 },
      "Visual Mode",
      0, 5, 1));

  // Color theme: 0=CalmBlue, 1=WarmSunset, 2=ForestMint, 3=HighContrast
  params.push_back(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID { kColorThemeParamId, 1 },
      "Color Theme",
      0, 3, 3)); // Default to High Contrast

  // Beats per bar: 1-16
  params.push_back(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID { kBeatsPerBarParamId, 1 },
      "Beats Per Bar",
      1, 16, 4));

  // Subdivisions: 1=1x, 2=2x, 3=3x, 4=4x
  params.push_back(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID { kSubdivisionsParamId, 1 },
      "Subdivisions",
      1, 4, 1));

  // Sound volume: 0.0 to 1.0
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID { kSoundVolumeParamId, 1 },
      "Sound Volume",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
      0.5f));

  // Preview subdivisions: whether to click on subdivision markers
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID { kPreviewSubdivisionsParamId, 1 },
      "Preview Subdivisions",
      false));

  return { params.begin(), params.end() };
}

VisualMode VizBeatsAudioProcessor::getVisualMode() const
{
  auto* param = apvts.getRawParameterValue(kVisualModeParamId);
  return static_cast<VisualMode>(static_cast<int>(param->load()));
}

ColorTheme VizBeatsAudioProcessor::getColorTheme() const
{
  auto* param = apvts.getRawParameterValue(kColorThemeParamId);
  return static_cast<ColorTheme>(static_cast<int>(param->load()));
}

int VizBeatsAudioProcessor::getBeatsPerBar() const
{
  auto* param = apvts.getRawParameterValue(kBeatsPerBarParamId);
  return static_cast<int>(param->load());
}

int VizBeatsAudioProcessor::getSubdivisions() const
{
  auto* param = apvts.getRawParameterValue(kSubdivisionsParamId);
  return static_cast<int>(param->load());
}

float VizBeatsAudioProcessor::getSoundVolume() const
{
  auto* param = apvts.getRawParameterValue(kSoundVolumeParamId);
  return param->load();
}

bool VizBeatsAudioProcessor::getPreviewSubdivisions() const
{
  auto* param = apvts.getRawParameterValue(kPreviewSubdivisionsParamId);
  return param->load() > 0.5f;
}

const juce::String VizBeatsAudioProcessor::getName() const
{
  return "VizBeats";
}

bool VizBeatsAudioProcessor::acceptsMidi() const
{
  return false;
}

bool VizBeatsAudioProcessor::producesMidi() const
{
  return false;
}

bool VizBeatsAudioProcessor::isMidiEffect() const
{
  return false;
}

double VizBeatsAudioProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int VizBeatsAudioProcessor::getNumPrograms()
{
  return 1;
}

int VizBeatsAudioProcessor::getCurrentProgram()
{
  return 0;
}

void VizBeatsAudioProcessor::setCurrentProgram(int)
{
}

const juce::String VizBeatsAudioProcessor::getProgramName(int)
{
  return {};
}

void VizBeatsAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void VizBeatsAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  sampleRateHz = sampleRate;
  internalSamplesPerBeat = sampleRateHz * (60.0 / juce::jmax(30.0, juce::jmin(300.0, hostBpm.load())));
  hostSamplesPerBeat = 0.0;
  hostLastSamplePos = 0.0;
  internalPhaseSamples = 0.0;
  clickPhaseDelta = juce::MathConstants<double>::twoPi * clickFreqStart / sampleRateHz;
  resetClick();
  juce::ignoreUnused(samplesPerBlock);
}

void VizBeatsAudioProcessor::releaseResources()
{
  resetClick();
}

bool VizBeatsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  const auto& mainIn = layouts.getMainInputChannelSet();
  const auto& mainOut = layouts.getMainOutputChannelSet();

  // Must have output
  if (mainOut.isDisabled())
    return false;

  // Accept mono or stereo output
  if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
    return false;

  // Input can be:
  // - Disabled (no input, we generate click only)
  // - Same as output (pass-through with click overlay)
  if (mainIn.isDisabled())
    return true;

  return mainIn == mainOut;
}

void VizBeatsAudioProcessor::updateHostInfo()
{
  bool isPlaying = false;

  bool hasBpm = false;
  double bpm = 120.0;

  bool hasPpqPosition = false;
  double ppqPosition = 0.0;

  if (auto* playHead = getPlayHead())
  {
    if (auto position = playHead->getPosition())
    {
      isPlaying = position->getIsPlaying();

      if (auto optBpm = position->getBpm())
      {
        bpm = *optBpm;
        hasBpm = std::isfinite(bpm) && bpm > 0.0;
      }

      if (auto optPpq = position->getPpqPosition())
      {
        ppqPosition = *optPpq;
        hasPpqPosition = std::isfinite(ppqPosition);
      }
    }
  }

  hostIsPlaying.store(isPlaying, std::memory_order_relaxed);
  hostHasBpm.store(hasBpm, std::memory_order_relaxed);
  hostBpm.store(bpm, std::memory_order_relaxed);
  hostHasPpqPosition.store(hasPpqPosition, std::memory_order_relaxed);
  hostPpqPosition.store(ppqPosition, std::memory_order_relaxed);
}

VizBeatsAudioProcessor::HostInfo VizBeatsAudioProcessor::getHostInfo() const noexcept
{
  HostInfo info;
  info.isPlaying = hostIsPlaying.load(std::memory_order_relaxed);
  info.hasBpm = hostHasBpm.load(std::memory_order_relaxed);
  info.bpm = hostBpm.load(std::memory_order_relaxed);
  info.hasPpqPosition = hostHasPpqPosition.load(std::memory_order_relaxed);
  info.ppqPosition = hostPpqPosition.load(std::memory_order_relaxed);
  return info;
}

void VizBeatsAudioProcessor::refreshHostInfo()
{
  updateHostInfo();
}

void VizBeatsAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
  juce::ScopedNoDenormals noDenormals;
  juce::ignoreUnused(midiMessages);

  updateHostInfo();

  const auto manualBpm = static_cast<double>(apvts.getRawParameterValue(kManualBpmParamId)->load());
  const auto internalPlay = apvts.getRawParameterValue(kInternalPlayParamId)->load() > 0.5f;
  const auto beatsPerBar = getBeatsPerBar();
  const auto subdivisions = getSubdivisions();
  const auto previewSubdivisions = getPreviewSubdivisions();
  const auto hostInfo = getHostInfo();
  const bool useInternalClock = internalPlay && !hostInfo.isPlaying;

  double beatPhase = 0.0;
  bool isRunning = false;

  const bool hasPhase = computeBeatPhase(beatPhase, isRunning, manualBpm, internalPlay, buffer.getNumSamples());

  if (isRunning && hasPhase)
  {
    const bool phaseWrapped = lastBeatPhaseValid && (beatPhase < lastBeatPhase);
    const bool shouldClick = !lastRunning || !lastBeatPhaseValid || phaseWrapped;
    bool barWrapped = false;

    // Different sound only when the ball reaches the right bar (bar wrap).
    if (useInternalClock)
    {
      lastBarProgressValid = false;
      lastBeatsPerBarForProgress = beatsPerBar;

      if (!lastInternalPlay)
        internalBeatCounter = 0;

      if (phaseWrapped)
        ++internalBeatCounter;

      lastInternalPlay = true;

      if (phaseWrapped && beatsPerBar > 0)
        barWrapped = (internalBeatCounter % static_cast<int64_t>(beatsPerBar) == 0);
    }
    else
    {
      lastInternalPlay = false;
      internalBeatCounter = 0;

      if (hostInfo.isPlaying && hostInfo.hasPpqPosition && beatsPerBar > 0)
      {
        if (lastBeatsPerBarForProgress != beatsPerBar)
        {
          lastBeatsPerBarForProgress = beatsPerBar;
          lastBarProgressValid = false;
        }

        const auto barBeats = static_cast<double>(beatsPerBar);
        auto inBar = std::fmod(hostInfo.ppqPosition, barBeats);
        if (inBar < 0.0)
          inBar += barBeats;
        const auto barProgress01 = inBar / barBeats;

        if (lastBarProgressValid)
          barWrapped = (barProgress01 + 0.15 < lastBarProgress01);

        lastBarProgress01 = barProgress01;
        lastBarProgressValid = true;
      }
      else
      {
        lastBeatsPerBarForProgress = beatsPerBar;
        lastBarProgressValid = false;
      }
    }

    if (shouldClick)
      triggerClick(phaseWrapped && barWrapped);

    lastBeatPhase = beatPhase;
    lastBeatPhaseValid = true;
  }
  else
  {
    lastBeatPhaseValid = false;
    lastBarProgressValid = false;
    lastInternalPlay = false;
    internalBeatCounter = 0;
    lastSubdivPhaseValid = false;
  }

  lastRunning = isRunning;

  const auto totalNumInputChannels = getTotalNumInputChannels();
  const auto totalNumOutputChannels = getTotalNumOutputChannels();
  const auto numSamples = buffer.getNumSamples();

  // If no input channels (generator mode), clear output first
  if (totalNumInputChannels == 0)
  {
    for (auto channel = 0; channel < totalNumOutputChannels; ++channel)
      buffer.clear(channel, 0, numSamples);
  }
  else
  {
    // Clear any extra output channels that don't have corresponding inputs
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
      buffer.clear(channel, 0, numSamples);
  }

  // Audio passes through unchanged (input already in buffer for in-place processing)
  // We just add our click sound on top

  if (!isRunning)
    clickSamplesLeft = 0;

  // Mix click sound into the output
  if (isRunning)
    renderClick(buffer);
}

bool VizBeatsAudioProcessor::computeBeatPhase(double& outPhase, bool& outRunning, double manualBpm, bool internalPlay, int numSamples)
{
  outRunning = false;

  const auto info = getHostInfo();
  const auto bpm = info.hasBpm ? info.bpm : manualBpm;

  if (info.isPlaying && info.hasPpqPosition)
  {
    outRunning = true;
    auto phase = info.ppqPosition - std::floor(info.ppqPosition);
    if (phase < 0.0)
      phase += 1.0;
    outPhase = phase;
    return true;
  }

  if (info.isPlaying && info.hasBpm)
  {
    // Fallback: use sample position if PPQ is missing
    if (auto* playHead = getPlayHead())
    {
      if (auto position = playHead->getPosition())
      {
        if (auto timeSamples = position->getTimeInSamples())
        {
          hostSamplesPerBeat = sampleRateHz * (60.0 / juce::jlimit(30.0, 300.0, bpm));
          const auto samples = static_cast<double>(*timeSamples);
          const auto wrapped = hostSamplesPerBeat > 0.0 ? std::fmod(samples, hostSamplesPerBeat) : 0.0;
          outPhase = hostSamplesPerBeat > 0.0 ? wrapped / hostSamplesPerBeat : 0.0;
          outRunning = true;
          hostLastSamplePos = samples;
          return true;
        }
      }
    }
  }

  if (internalPlay)
  {
    outRunning = true;
    internalSamplesPerBeat = sampleRateHz * (60.0 / juce::jlimit(30.0, 300.0, bpm));

    if (internalSamplesPerBeat > 0.0)
    {
      const auto wrappedSamples = std::fmod(internalPhaseSamples, internalSamplesPerBeat);
      outPhase = wrappedSamples / internalSamplesPerBeat;
    }
    else
    {
      outPhase = 0.0;
    }

    internalPhaseSamples += static_cast<double>(numSamples);
    return true;
  }

  internalPhaseSamples = 0.0;
  return false;
}

void VizBeatsAudioProcessor::resetClick()
{
  clickSamplesLeft = 0;
  lastBeatPhaseValid = false;
  lastRunning = false;
  lastBarProgressValid = false;
  lastBarProgress01 = 0.0;
  lastBeatsPerBarForProgress = 0;
  internalPhaseSamples = 0.0;
  internalBeatCounter = 0;
  lastInternalPlay = false;
  lastSubdivPhaseValid = false;
  lastSubdivPhase = 0.0;
}

void VizBeatsAudioProcessor::triggerClick(bool accent)
{
  clickSamplesLeft = clickLengthSamples;
  clickPhase = 0.0;

  if (accent)
  {
    clickGainCurrent = 0.60f;
    clickFreqStartCurrent = 2800.0;
    clickFreqEndCurrent = 1100.0;
  }
  else
  {
    clickGainCurrent = clickGain;
    clickFreqStartCurrent = clickFreqStart;
    clickFreqEndCurrent = clickFreqEnd;
  }
}

void VizBeatsAudioProcessor::triggerSubdivisionClick()
{
  // Softer, higher-pitched click for subdivisions
  clickSamplesLeft = static_cast<int>(clickLengthSamples * 0.6); // Shorter click
  clickPhase = 0.0;
  clickGainCurrent = clickGain * 0.35f; // Much softer
  clickFreqStartCurrent = 3200.0; // Higher pitch
  clickFreqEndCurrent = 1800.0;
}

void VizBeatsAudioProcessor::renderClick(juce::AudioBuffer<float>& buffer)
{
  if (clickSamplesLeft <= 0 || sampleRateHz <= 0.0)
    return;

  const auto numSamples = buffer.getNumSamples();
  const auto numCh = buffer.getNumChannels();
  const auto volume = getSoundVolume();

  for (int i = 0; i < numSamples; ++i)
  {
    if (clickSamplesLeft <= 0)
      break;

    const auto t = 1.0 - static_cast<double>(clickSamplesLeft) / static_cast<double>(clickLengthSamples);
    const auto freq = clickFreqStartCurrent + (clickFreqEndCurrent - clickFreqStartCurrent) * t;
    clickPhaseDelta = juce::MathConstants<double>::twoPi * freq / sampleRateHz;

    const auto env = static_cast<float>(std::exp(-5.0 * t));
    const auto tone = static_cast<float>(std::sin(clickPhase));
    const auto sample = clickGainCurrent * volume * env * tone;
    clickPhase += clickPhaseDelta;

    for (int ch = 0; ch < numCh; ++ch)
      buffer.addSample(ch, i, sample);

    --clickSamplesLeft;
  }
}

bool VizBeatsAudioProcessor::hasEditor() const
{
  return true;
}

juce::AudioProcessorEditor* VizBeatsAudioProcessor::createEditor()
{
  return new VizBeatsAudioProcessorEditor(*this);
}

void VizBeatsAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  juce::MemoryOutputStream stream(destData, false);
  apvts.state.writeToStream(stream);
}

void VizBeatsAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
  if (state.isValid())
    apvts.replaceState(state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new VizBeatsAudioProcessor();
}
