#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
constexpr auto kManualBpmParamId = "manualBpm";
constexpr auto kInternalPlayParamId = "internalPlay";
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

  return { params.begin(), params.end() };
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

  if (mainIn.isDisabled() || mainOut.isDisabled())
    return false;

  if (mainIn != mainOut)
    return false;

  return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
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

  double beatPhase = 0.0;
  bool isRunning = false;

  const bool hasPhase = computeBeatPhase(beatPhase, isRunning, manualBpm, internalPlay, buffer.getNumSamples());

  if (isRunning && hasPhase)
  {
    const bool wrapped = !lastBeatPhaseValid || (beatPhase < lastBeatPhase);
    if (!lastRunning || wrapped)
      triggerClick();

    lastBeatPhase = beatPhase;
    lastBeatPhaseValid = true;
  }
  else
  {
    lastBeatPhaseValid = false;
  }

  lastRunning = isRunning;

  const auto totalNumInputChannels = getTotalNumInputChannels();
  const auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
    buffer.clear(channel, 0, buffer.getNumSamples());

  if (!isRunning)
    clickSamplesLeft = 0;

  // mix click after clearing unused outputs so we don't wipe it out
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
  internalPhaseSamples = 0.0;
}

void VizBeatsAudioProcessor::triggerClick()
{
  clickSamplesLeft = clickLengthSamples;
  clickPhase = 0.0;
}

void VizBeatsAudioProcessor::renderClick(juce::AudioBuffer<float>& buffer)
{
  if (clickSamplesLeft <= 0 || sampleRateHz <= 0.0)
    return;

  const auto numSamples = buffer.getNumSamples();
  const auto numCh = buffer.getNumChannels();

  for (int i = 0; i < numSamples; ++i)
  {
    if (clickSamplesLeft <= 0)
      break;

    const auto t = 1.0 - static_cast<double>(clickSamplesLeft) / static_cast<double>(clickLengthSamples);
    const auto freq = clickFreqStart + (clickFreqEnd - clickFreqStart) * t;
    clickPhaseDelta = juce::MathConstants<double>::twoPi * freq / sampleRateHz;

    const auto env = static_cast<float>(std::exp(-5.0 * t));
    const auto tone = static_cast<float>(std::sin(clickPhase));
    const auto sample = clickGain * env * tone;
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
