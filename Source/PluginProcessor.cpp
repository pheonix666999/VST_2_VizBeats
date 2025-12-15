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
      120.0f));

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

void VizBeatsAudioProcessor::prepareToPlay(double, int)
{
}

void VizBeatsAudioProcessor::releaseResources()
{
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

  const auto totalNumInputChannels = getTotalNumInputChannels();
  const auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
    buffer.clear(channel, 0, buffer.getNumSamples());
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
