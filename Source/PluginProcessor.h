#pragma once

#include <JuceHeader.h>

// Visual mode enumeration
enum class VisualMode
{
  Pulse = 0,
  Traffic,
  Pendulum,
  Bounce,
  Ladder,
  Pattern
};

// Color theme enumeration
enum class ColorTheme
{
  CalmBlue = 0,
  WarmSunset,
  ForestMint,
  HighContrast
};

class VizBeatsAudioProcessor final : public juce::AudioProcessor
{
public:
  VizBeatsAudioProcessor();
  ~VizBeatsAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  struct HostInfo
  {
    bool isPlaying = false;
    bool hasBpm = false;
    double bpm = 120.0;
    bool hasPpqPosition = false;
    double ppqPosition = 0.0;
  };

  HostInfo getHostInfo() const noexcept;
  void refreshHostInfo();

  // Helper methods to get settings
  VisualMode getVisualMode() const;
  ColorTheme getColorTheme() const;
  int getBeatsPerBar() const;
  int getSubdivisions() const;
  float getSoundVolume() const;
  bool getPreviewSubdivisions() const;

  juce::AudioProcessorValueTreeState apvts;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
  void updateHostInfo();
  void resetClick();
  void triggerClick(bool accent);
  void triggerSubdivisionClick();
  void renderClick(juce::AudioBuffer<float>& buffer);
  bool computeBeatPhase(double& outPhase, bool& outRunning, double manualBpm, bool internalPlay, int numSamples);

  std::atomic<bool> hostIsPlaying { false };
  std::atomic<bool> hostHasBpm { false };
  std::atomic<double> hostBpm { 120.0 };
  std::atomic<bool> hostHasPpqPosition { false };
  std::atomic<double> hostPpqPosition { 0.0 };

  double sampleRateHz = 44100.0;
  double internalPhaseSamples = 0.0;
  double internalSamplesPerBeat = 44100.0;
  double hostSamplesPerBeat = 0.0;
  double hostLastSamplePos = 0.0;
  bool hostFallbackRunning = false;
  double hostFallbackPhaseSamples = 0.0;

	  double lastBeatPhase = 0.0;
	  bool lastBeatPhaseValid = false;
	  bool lastRunning = false;
	  double lastBarProgress01 = 0.0;
	  bool lastBarProgressValid = false;
	  int lastBeatsPerBarForProgress = 0;

  int clickSamplesLeft = 0;
  int clickLengthSamples = 1764; // ~40 ms at 44.1k
  float clickGain = 0.45f;
  float clickGainCurrent = 0.45f;
  double clickPhase = 0.0;
  double clickPhaseDelta = 0.0;
  double clickFreqStart = 2200.0;
  double clickFreqEnd = 800.0;
  double clickFreqStartCurrent = 2200.0;
  double clickFreqEndCurrent = 800.0;
  juce::Random rand;

  double lastClickBpm = 120.0;
  bool lastInternalPlay = false;
  int64_t internalBeatCounter = 0;

  // Subdivision tracking
  double lastSubdivPhase = 0.0;
  bool lastSubdivPhaseValid = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VizBeatsAudioProcessor)
};
