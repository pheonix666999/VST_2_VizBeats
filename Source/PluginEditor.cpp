#include "PluginEditor.h"
#include "PluginProcessor.h"

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

// Color themes
struct ThemeColors
{
  juce::Colour background;
  juce::Colour accent;
  juce::Colour accentSecondary;
  juce::Colour textPrimary;
  juce::Colour textMuted;
  juce::Colour panelBg;
  juce::Colour barMarker;
};

ThemeColors getThemeColors(ColorTheme theme)
{
  switch (theme)
  {
    case ColorTheme::CalmBlue:
      return {
        juce::Colour(0xff0b1323), // background
        juce::Colour(0xff32b7ff), // accent
        juce::Colour(0xff2da5ff), // accentSecondary
        juce::Colour(0xffffffff), // textPrimary
        juce::Colour(0xff9aa8bd), // textMuted
        juce::Colour(0xff1e2b3e), // panelBg
        juce::Colour(0xff32b7ff)  // barMarker
      };
    case ColorTheme::WarmSunset:
      return {
        juce::Colour(0xff1a1210), // background
        juce::Colour(0xffff9a3c), // accent (orange)
        juce::Colour(0xffff7b2e), // accentSecondary
        juce::Colour(0xffffffff), // textPrimary
        juce::Colour(0xffbda99a), // textMuted
        juce::Colour(0xff2e201e), // panelBg
        juce::Colour(0xffff9a3c)  // barMarker
      };
    case ColorTheme::ForestMint:
      return {
        juce::Colour(0xff0b1a14), // background
        juce::Colour(0xff3cffaa), // accent (mint)
        juce::Colour(0xff2ee89a), // accentSecondary
        juce::Colour(0xffffffff), // textPrimary
        juce::Colour(0xff9abda8), // textMuted
        juce::Colour(0xff1e2e28), // panelBg
        juce::Colour(0xff3cffaa)  // barMarker
      };
    case ColorTheme::HighContrast:
    default:
      return {
        juce::Colour(0xff000000), // background (pure black)
        juce::Colour(0xffffffff), // accent (white)
        juce::Colour(0xffcccccc), // accentSecondary
        juce::Colour(0xffffffff), // textPrimary
        juce::Colour(0xff888888), // textMuted
        juce::Colour(0xff1a1a1a), // panelBg
        juce::Colour(0xffffffff)  // barMarker (white)
      };
  }
}

static juce::Path makeGearPath()
{
  juce::Path p;
  const float cx = 12.0f, cy = 12.0f;
  const float outerR = 10.0f;
  const float innerR = 7.0f;
  const float holeR = 3.0f;
  const int numTeeth = 6;
  const float toothDepth = 2.5f;
  const float toothWidth = 0.4f; // radians

  // Create gear outline with teeth
  for (int i = 0; i < numTeeth; ++i)
  {
    float angle = juce::MathConstants<float>::twoPi * i / numTeeth;
    float nextAngle = juce::MathConstants<float>::twoPi * (i + 1) / numTeeth;
    float midAngle = (angle + nextAngle) / 2.0f;

    // Tooth start (inner)
    float a1 = midAngle - toothWidth;
    float a2 = midAngle + toothWidth;

    if (i == 0)
      p.startNewSubPath(cx + innerR * std::cos(angle), cy + innerR * std::sin(angle));

    // Line to tooth base
    p.lineTo(cx + innerR * std::cos(a1), cy + innerR * std::sin(a1));
    // Line to tooth top
    p.lineTo(cx + outerR * std::cos(a1), cy + outerR * std::sin(a1));
    p.lineTo(cx + outerR * std::cos(a2), cy + outerR * std::sin(a2));
    // Line back to inner
    p.lineTo(cx + innerR * std::cos(a2), cy + innerR * std::sin(a2));
    // Line to next tooth start
    p.lineTo(cx + innerR * std::cos(nextAngle), cy + innerR * std::sin(nextAngle));
  }
  p.closeSubPath();

  // Add center hole
  p.addEllipse(cx - holeR, cy - holeR, holeR * 2.0f, holeR * 2.0f);
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
  // Fast flash per beat: strong at phase=0, quickly decays (closer to original demo).
  const auto decay = std::exp(-20.0f * phase);
  return clamp01(static_cast<float>(decay * decay) * 1.15f);
}

// Icon Button
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

// Step Button (+/-)
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

// Play/Pause Button
class PlayPauseButton final : public juce::Button
{
public:
  PlayPauseButton()
      : juce::Button("")
  {
    setClickingTogglesState(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void setAccentColor(juce::Colour c) { accentColor = c; repaint(); }

  void paintButton(juce::Graphics& g, bool shouldDrawHover, bool isDown) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto circle = bounds.withSizeKeepingCentre(size, size);

    const auto fill = accentColor.brighter((shouldDrawHover ? 0.08f : 0.0f) + (isDown ? 0.05f : 0.0f));

    g.setColour(fill);
    g.fillEllipse(circle);

    g.setColour(juce::Colours::black.withAlpha(0.92f));

    auto iconBounds = circle.reduced(circle.getWidth() * 0.30f);
    const auto icon = getToggleState() ? makePausePath() : makePlayPath();
    g.fillPath(icon, icon.getTransformToScaleToFit(iconBounds, true));
  }

private:
  juce::Colour accentColor { 0xff32b7ff };
};

// BPM Readout
class BpmReadout final : public juce::Component
{
public:
  void setBpm(double bpmToShow)
  {
    bpm = bpmToShow;
    repaint();
  }

  void setColors(juce::Colour primary, juce::Colour muted)
  {
    textColor = primary;
    mutedColor = muted;
    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds();

    const auto bpmInt = static_cast<int>(std::round(bpm));
    const auto numberArea = bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.68f));
    const auto labelArea = bounds;

    g.setColour(textColor.withAlpha(0.95f));
    g.setFont(juce::Font(numberArea.getHeight() * 0.70f, juce::Font::bold));
    g.drawText(juce::String(bpmInt), numberArea, juce::Justification::centred);

    g.setColour(mutedColor.withAlpha(0.85f));
    g.setFont(juce::Font(labelArea.getHeight() * 0.60f, juce::Font::plain));
    g.drawText("BPM", labelArea, juce::Justification::centred);
  }

private:
  double bpm = 120.0;
  juce::Colour textColor { 0xffffffff };
  juce::Colour mutedColor { 0xff9aa8bd };
};

// Option Button for settings panel
class OptionButton final : public juce::Button
{
public:
  OptionButton(juce::String text, bool hasIndicator = false, juce::Colour indicatorColor = juce::Colours::white)
      : juce::Button(""), label(std::move(text)), showIndicator(hasIndicator), indicatorCol(indicatorColor)
  {
    setClickingTogglesState(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void setIndicatorColor(juce::Colour c) { indicatorCol = c; repaint(); }
  void setTheme(ThemeColors t) { theme = t; repaint(); }

  void paintButton(juce::Graphics& g, bool shouldDrawHover, bool isDown) override
  {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const bool selected = getToggleState();

    // Background
    if (selected)
      g.setColour(theme.panelBg.brighter(0.18f));
    else if (shouldDrawHover)
      g.setColour(theme.panelBg.brighter(0.10f));
    else
      g.setColour(theme.panelBg.withAlpha(0.95f));

    g.fillRoundedRectangle(bounds, 8.0f);

    // Border for selected
    if (selected)
    {
      g.setColour(theme.accent.withAlpha(0.35f));
      g.drawRoundedRectangle(bounds, 8.0f, 1.5f);
    }

    // Indicator circle
    float textStartX = 12.0f;
    if (showIndicator)
    {
      g.setColour(indicatorCol);
      g.fillEllipse(12.0f, bounds.getCentreY() - 5.0f, 10.0f, 10.0f);
      textStartX = 30.0f;
    }

    // Text
    g.setColour((selected ? theme.textPrimary : theme.textMuted).withAlpha(selected ? 0.98f : 0.92f));
    g.setFont(juce::Font(14.0f, juce::Font::plain));
    auto textBounds = bounds.withLeft(textStartX).withRight(bounds.getRight() - 8.0f);
    g.drawText(label, textBounds, juce::Justification::centredLeft);
  }

private:
  juce::String label;
  bool showIndicator;
  juce::Colour indicatorCol;
  ThemeColors theme = getThemeColors(ColorTheme::HighContrast);
};

} // namespace

//==============================================================================
// Pulse Visualizer
//==============================================================================
class VizBeatsAudioProcessorEditor::PulseVisualizer final : public juce::Component
{
public:
  void setPulse(float newPulse)
  {
    const float target = clamp01(newPulse);
    // Snappier response (fast flash per beat).
    constexpr float smoothing = 0.28f;
    smoothedPulse = smoothedPulse * (1.0f - smoothing) + target * smoothing;
  }

  void setRunning(bool shouldRun) { running = shouldRun; }
  void setColors(ThemeColors colors) { theme = colors; repaint(); }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat();

    // Fill background with theme color
    g.setColour(theme.background);
    g.fillRect(bounds);

    const auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();

    const auto maxRadius = size * 0.60f;
    const auto minRadius = size * 0.10f;

    const auto decay = (running ? smoothedPulse : 0.0f);
    const auto radius = minRadius + (maxRadius - minRadius) * decay;

    const auto alpha = 0.14f + 0.82f * decay;
    const auto stroke = juce::jlimit(1.4f, 9.5f, maxRadius * (0.010f + 0.024f * decay));

    const auto ringBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);

    g.setColour(theme.accent.withAlpha(alpha * 0.42f));
    g.fillEllipse(ringBounds);

    g.setColour(theme.accent.withAlpha(alpha * 0.78f));
    g.drawEllipse(ringBounds, stroke * 1.05f);

    // soft glow behind the core
    const auto glowRadius = size * 0.11f;
    auto glowBounds = juce::Rectangle<float>(glowRadius * 2.0f, glowRadius * 2.0f).withCentre(centre);
    juce::ColourGradient glow(theme.accent.withAlpha(0.28f * decay), centre, theme.accent.withAlpha(0.0f), glowBounds.getBottomRight(), true);
    g.setGradientFill(glow);
    g.fillEllipse(glowBounds);

    const auto coreRadius = size * 0.085f;
    const auto coreBounds = juce::Rectangle<float>(coreRadius * 2.0f, coreRadius * 2.0f).withCentre(centre);

    g.setColour(theme.accentSecondary.withAlpha(0.97f));
    g.drawEllipse(coreBounds, stroke * 0.65f);

    const auto dotRadius = juce::jmax(2.6f, size * 0.011f);
    g.fillEllipse(juce::Rectangle<float>(dotRadius * 2.0f, dotRadius * 2.0f).withCentre(centre));
  }

private:
  float smoothedPulse = 1.0f;
  bool running = false;
  ThemeColors theme = getThemeColors(ColorTheme::HighContrast);
};

//==============================================================================
// Traffic Visualizer - horizontal beat timeline with marker interactions
//==============================================================================
class VizBeatsAudioProcessorEditor::TrafficVisualizer final : public juce::Component
{
public:
  void setBeatPhase(double phase) { beatPhase = phase; }
  void setRunning(bool shouldRun) { running = shouldRun; }
  void setBeatsPerBar(int beats) { beatsPerBar = juce::jmax(1, beats); }
  void setSubdivisions(int subs) { subdivisions = juce::jmax(1, subs); }
  void setCurrentBeat(int beat) { currentBeat = beat; }
  void setColors(ThemeColors colors) { theme = colors; repaint(); }

	  void paint(juce::Graphics& g) override
	  {
	    const auto nowMs = juce::Time::getMillisecondCounter();
	    float dtSeconds = 0.0f;
	    if (lastPaintTimeMs != 0)
	      dtSeconds = juce::jlimit(0.0f, 0.10f, static_cast<float>(nowMs - lastPaintTimeMs) * 0.001f);
	    lastPaintTimeMs = nowMs;

	    auto bounds = getLocalBounds().toFloat();

	    // Background: use the theme base colour (no left-side brightening).
	    g.setColour(theme.background);
	    g.fillRect(bounds);

	    const auto centreY = bounds.getCentreY();
	    const auto padding = 88.0f;
	    const auto lineStartX = padding;
	    const auto lineEndX = bounds.getWidth() - padding;
	    const auto lineWidth = lineEndX - lineStartX;
	    const float barWidth = 5.0f;
	    const float barHeight = 120.0f;
	    const float barY = centreY - barHeight * 0.5f;

    // Total visual segments (beats * subdivisions for visual markers)
    const int totalSegments = juce::jmax(1, beatsPerBar * subdivisions);
    const float markerSpacing = lineWidth / static_cast<float>(totalSegments);

    // Main beat segments (for ripples only)
    const int mainBeatSegments = juce::jmax(1, beatsPerBar);

    // Orb position: constant speed across the whole bar.
    const float barProgress01 = running
                                  ? clamp01(static_cast<float>((static_cast<double>(currentBeat) + beatPhase) / static_cast<double>(beatsPerBar)))
                                  : 0.0f;

	    // Track orb position relative to main beats only (for ripples)
	    const float orbMainBeatSpace = barProgress01 * static_cast<float>(mainBeatSegments);
	    const float orbX = lineStartX + barProgress01 * lineWidth;
	    const float orbY = centreY;

	    // Orb brightens near main beat markers only ("activation" feel).
	    const float distToNearest = std::abs(orbMainBeatSpace - std::round(orbMainBeatSpace));
	    const float hit = running ? std::exp(-distToNearest * distToNearest * 110.0f) : 0.0f;

	    // Decaying left flash pulse (triggered on bar wrap).
	    if (dtSeconds > 0.0f && leftFlash > 0.0f)
	    {
	      leftFlash *= std::exp(-dtSeconds * 6.5f);
	      if (leftFlash < 0.001f)
	        leftFlash = 0.0f;
	    }

	    bool barWrapped = false;
	    if (running)
	    {
	      if (lastBarProgress01 >= 0.0f)
	        barWrapped = (barProgress01 + 0.10f < lastBarProgress01);

	      lastBarProgress01 = barProgress01;

	      if (barWrapped)
	        leftFlash = 1.0f;
	    }
	    else
	    {
	      lastBarProgress01 = -1.0f;
	      leftFlash = 0.0f;
	    }

	    const float leftPulse = leftFlash;

	    // Keep a small "hit" pulse at the right bar for the orb only (right bar itself should not light up).
	    float rightPulse = 0.0f;
	    if (running)
	    {
	      const auto pulseFromDistance = [](float distPx, float sigmaPx)
	      {
	        if (sigmaPx <= 0.0f)
	          return 0.0f;

	        const float x = distPx / sigmaPx;
	        float v = std::exp(-0.5f * x * x);
	        v *= v;
	        return clamp01(v);
	      };

	      const auto gate = [](float v, float threshold)
	      {
	        if (v <= threshold)
	          return 0.0f;
	        return clamp01((v - threshold) / (1.0f - threshold));
	      };

	      const float rightHit = pulseFromDistance(std::abs(orbX - lineEndX), 14.0f);
	      rightPulse = gate(rightHit, 0.10f);
	    }

	    // Left-side screen flash (momentary) when the ball reaches the left bar.
	    if (leftPulse > 0.0f)
	    {
	      const float intensity = clamp01(leftPulse);
	      const float leftBarX = lineStartX - barWidth - 10.0f;
	      const float entryX = leftBarX + barWidth * 0.5f;
	      const float entryY = centreY;

	      const auto bgArgb = theme.background.getARGB();
	      const auto accentArgb = theme.accent.getARGB();
	      const auto accent2Argb = theme.accentSecondary.getARGB();
	      const auto flashKey =
	          (static_cast<juce::uint64>(bgArgb) << 32)
	          ^ static_cast<juce::uint64>(accentArgb)
	          ^ (static_cast<juce::uint64>(accent2Argb) << 1);

	      const int w = juce::jmax(1, static_cast<int>(std::round(bounds.getWidth())));
	      const int h = juce::jmax(1, static_cast<int>(std::round(bounds.getHeight())));

	      if (leftFlashOverlay.isNull() || leftFlashOverlay.getWidth() != w || leftFlashOverlay.getHeight() != h || leftFlashOverlayKey != flashKey)
	      {
	        leftFlashOverlayKey = flashKey;
	        leftFlashOverlay = juce::Image(juce::Image::ARGB, w, h, true);

	        juce::Image::BitmapData bd(leftFlashOverlay, juce::Image::BitmapData::writeOnly);

	        const float wf = static_cast<float>(w);
	        const float hf = static_cast<float>(h);

	        // Smooth ambient wash - gradual fade from left edge to center
	        const float fadeWidth = wf * 0.45f; // How far the glow extends horizontally

	        const float ditherScale = 1.5f / 255.0f;
	        const auto seed = static_cast<juce::uint32>((flashKey >> 32) ^ (flashKey & 0xffffffffu));
	        juce::Random rng(static_cast<int>(seed));

	        for (int py = 0; py < h; ++py)
	        {
	          for (int px = 0; px < w; ++px)
	          {
	            const float x = static_cast<float>(px);

	            // Simple horizontal fade from left edge
	            float horizontalFade = 1.0f - (x / fadeWidth);
	            horizontalFade = juce::jmax(0.0f, horizontalFade);
	            // Smooth cubic falloff for natural look
	            horizontalFade = horizontalFade * horizontalFade * (3.0f - 2.0f * horizontalFade);

	            float a = horizontalFade * 0.50f; // Max alpha at left edge - more kick!

	            if (a < 0.002f)
	            {
	              bd.setPixelColour(px, py, juce::Colours::transparentBlack);
	              continue;
	            }

	            // Add subtle dithering to prevent banding
	            a += (rng.nextFloat() - 0.5f) * ditherScale;
	            a = clamp01(a);

	            bd.setPixelColour(px, py, theme.accent.withAlpha(a));
	          }
	        }
	      }

	      juce::Graphics::ScopedSaveState state(g);
	      g.setOpacity(intensity * 0.85f);
	      g.drawImageAt(leftFlashOverlay, 0, 0);
	    }

	    // Baseline line (draw after possible wash so it stays crisp).
	    g.setColour(theme.textMuted.withAlpha(0.22f));
	    g.drawLine(lineStartX, centreY, lineEndX, centreY, 1.0f);

	    // Side bars.
	    auto drawSideBar = [&](float x, bool isLeft, float pulse)
	    {
	      // Keep both bars the same colour; only the left bar receives a hit overlay.
	      constexpr float baseAlpha = 0.35f;
	      g.setColour(theme.barMarker.withAlpha(baseAlpha));
	      g.fillRect(x, barY, barWidth, barHeight);

      pulse = clamp01(pulse);
      if (pulse <= 0.0f)
        return;

      // Subtle bar highlight when hit
      const float overlayAlpha = (isLeft ? 0.25f : 0.95f) * pulse;
      g.setColour(theme.barMarker.withAlpha(overlayAlpha));
      g.fillRect(x, barY, barWidth, barHeight);
    };

	    drawSideBar(lineStartX - barWidth - 10.0f, true, leftPulse);
	    drawSideBar(lineEndX + 10.0f, false, 0.0f);

    // Ripples: emit when orb passes a MAIN BEAT marker only (not subdivisions).
    const float mainBeatMarkerSpacing = lineWidth / static_cast<float>(mainBeatSegments);
    if (running)
    {
      if (lastOrbMarkerSpace < 0.0f)
      {
        lastOrbMarkerSpace = orbMainBeatSpace;
      }
      else
      {
        const bool wrapped = orbMainBeatSpace + 0.25f < lastOrbMarkerSpace;
        if (wrapped)
        {
          ripples.clear();
        }
        else
        {
          const int lastIdx = static_cast<int>(std::floor(lastOrbMarkerSpace));
          const int nowIdx = static_cast<int>(std::floor(orbMainBeatSpace));
          if (nowIdx > lastIdx)
          {
            for (int i = lastIdx + 1; i <= nowIdx; ++i)
            {
              // Ripples only at main beat positions
              const float x = lineStartX + static_cast<float>(juce::jlimit(0, mainBeatSegments, i)) * mainBeatMarkerSpacing;
              ripples.push_back({ x, centreY, 0.0f });
            }
          }
        }
        lastOrbMarkerSpace = orbMainBeatSpace;
      }
    }
    else
    {
      lastOrbMarkerSpace = -1.0f;
      ripples.clear();
    }

    // Draw all visual markers (main beats + subdivisions)
    for (int i = 0; i <= totalSegments; ++i)
    {
      const float x = lineStartX + static_cast<float>(i) * markerSpacing;

      // No end-circles near the side bars (matches reference).
      if (i == 0 || i == totalSegments)
        continue;

      // Main beats are larger, subdivisions are smaller
      const bool isMainBeat = (i % subdivisions) == 0;
      const float size = isMainBeat ? 7.0f : 4.5f;
      const float stroke = isMainBeat ? 1.3f : 0.9f;
      const float alpha = isMainBeat ? 0.35f : 0.20f;

      // ring markers stay static (no proximity glow); ripples handle "hit" feedback.
      g.setColour(theme.textMuted.withAlpha(alpha));
      g.drawEllipse(x - size * 0.5f, centreY - size * 0.5f, size, size, stroke);
    }

    // Draw ripples after markers.
    if (!ripples.empty())
    {
      constexpr float lifeSeconds = 0.28f;
      const float speed = 120.0f; // px/sec

      for (auto& r : ripples)
        r.ageSeconds += dtSeconds;

      ripples.erase(
          std::remove_if(ripples.begin(), ripples.end(), [lifeSeconds](const Ripple& r) { return r.ageSeconds >= lifeSeconds; }),
          ripples.end());

      for (const auto& r : ripples)
      {
        const float t = clamp01(r.ageSeconds / lifeSeconds);
        const float radius = 4.0f + t * speed * lifeSeconds;
        const float alpha = (1.0f - t);
        const float stroke = 4.4f - 1.4f * t;

        // Outer ring (more prominent)
        g.setColour(theme.accent.withAlpha(0.52f * alpha));
        g.drawEllipse(r.x - radius, r.y - radius, radius * 2.0f, radius * 2.0f, stroke);

        // Inner highlight
        g.setColour(juce::Colours::white.withAlpha(0.18f * alpha));
        g.drawEllipse(r.x - radius * 0.82f, r.y - radius * 0.82f, radius * 1.64f, radius * 1.64f, stroke * 0.62f);

        // Soft halo
        g.setColour(theme.accent.withAlpha(0.20f * alpha));
        g.drawEllipse(r.x - radius * 1.12f, r.y - radius * 1.12f, radius * 2.24f, radius * 2.24f, stroke * 0.55f);
      }
    }

    // Orb
    if (running)
    {
      // Orb should not glow all the time: keep glow mostly tied to "hits".
      const float glowAmt = 0.05f + 0.70f * hit + 0.20f * (leftPulse + rightPulse);

      // Tail: starts at zero length and grows.
      const float progress01 = barProgress01;
      const float tailLength = 115.0f * progress01;
      const float tailHeight = 5.6f + 3.2f * progress01;

      const float tailStartX = juce::jmax(lineStartX, orbX - tailLength);
      const float tailEndX = orbX - 8.0f;
      if (tailEndX > tailStartX)
      {
        // Tapered streak (sharper, like the reference).
        const float halfEnd = tailHeight * 0.52f;
        const float halfStart = tailHeight * 0.22f;

        const float tailAmt = juce::jlimit(0.0f, 1.0f, 0.22f + 0.78f * progress01 + 0.22f * hit);

        const auto tailCol = theme.accent.darker(0.45f);

        // Soft outer plume
        {
          const float oEnd = halfEnd * 1.45f;
          const float oStart = halfStart * 1.35f;

          juce::Path plume;
          plume.startNewSubPath(tailStartX, orbY - oStart);
          plume.lineTo(tailEndX, orbY - oEnd);
          plume.lineTo(tailEndX, orbY + oEnd);
          plume.lineTo(tailStartX, orbY + oStart);
          plume.closeSubPath();

          juce::ColourGradient plumeGrad(
              tailCol.withAlpha(0.18f * tailAmt),
              tailEndX,
              orbY,
              tailCol.withAlpha(0.0f),
              tailStartX,
              orbY,
              false);
          g.setGradientFill(plumeGrad);
          g.fillPath(plume);
        }

        juce::Path streak;
        streak.startNewSubPath(tailStartX, orbY - halfStart);
        streak.lineTo(tailEndX, orbY - halfEnd);
        streak.lineTo(tailEndX, orbY + halfEnd);
        streak.lineTo(tailStartX, orbY + halfStart);
        streak.closeSubPath();

        juce::ColourGradient streakGlow(
            tailCol.withAlpha(0.30f * tailAmt),
            tailEndX,
            orbY,
            tailCol.withAlpha(0.0f),
            tailStartX,
            orbY,
            false);
        g.setGradientFill(streakGlow);
        g.fillPath(streak);

        juce::Path core;
        const float coreHalfEnd = halfEnd * 0.32f;
        const float coreHalfStart = halfStart * 0.28f;
        core.startNewSubPath(tailStartX, orbY - coreHalfStart);
        core.lineTo(tailEndX, orbY - coreHalfEnd);
        core.lineTo(tailEndX, orbY + coreHalfEnd);
        core.lineTo(tailStartX, orbY + coreHalfStart);
        core.closeSubPath();

        juce::ColourGradient streakCore(
            theme.accentSecondary.withAlpha(0.18f * tailAmt),
            tailEndX,
            orbY,
            theme.accentSecondary.withAlpha(0.0f),
            tailStartX,
            orbY,
            false);
        g.setGradientFill(streakCore);
        g.fillPath(core);
      }

      // Minimal orb halo only when needed.
      if (glowAmt > 0.08f)
      {
        const float halo = 34.0f + 26.0f * glowAmt;
        juce::ColourGradient haloG(
            theme.accent.withAlpha(0.22f * glowAmt),
            orbX,
            orbY,
            theme.accent.withAlpha(0.0f),
            orbX + halo * 0.5f,
            orbY,
            true);
        g.setGradientFill(haloG);
        g.fillEllipse(orbX - halo * 0.5f, orbY - halo * 0.5f, halo, halo);
      }

      const float orbSize = 16.0f;
      g.setColour(juce::Colours::white.withAlpha(0.97f));
      g.fillEllipse(orbX - orbSize * 0.5f, orbY - orbSize * 0.5f, orbSize, orbSize);

      // Tiny accent edge to tie into theme.
      g.setColour(theme.accent.withAlpha(0.20f + 0.25f * glowAmt));
      g.drawEllipse(orbX - orbSize * 0.5f, orbY - orbSize * 0.5f, orbSize, orbSize, 1.2f);
    }
  }

	private:
	  struct Ripple
	  {
	    float x = 0.0f;
	    float y = 0.0f;
	    float ageSeconds = 0.0f;
	  };

	  double beatPhase = 0.0;
	  bool running = false;
	  int beatsPerBar = 4;
	  int subdivisions = 1;
	  int currentBeat = 0;
	  ThemeColors theme = getThemeColors(ColorTheme::HighContrast);

	  juce::uint32 lastPaintTimeMs = 0;
	  float leftFlash = 0.0f;
	  float lastBarProgress01 = -1.0f;
	  float lastOrbMarkerSpace = -1.0f;

	  juce::Image leftFlashOverlay;
	  juce::uint64 leftFlashOverlayKey = 0;

	  std::vector<Ripple> ripples;
	};

//==============================================================================
// Settings Panel
//==============================================================================
class VizBeatsAudioProcessorEditor::SettingsPanel final : public juce::Component
{
public:
  explicit SettingsPanel(VizBeatsAudioProcessor& p)
      : processor(p)
  {
    // Visual Mode buttons
    for (int i = 0; i < 6; ++i)
    {
      const char* labels[] = { "Pulse", "Traffic", "Pendulum", "Bounce", "Ladder", "Pattern" };
      auto btn = std::make_unique<OptionButton>(labels[i]);
      btn->setRadioGroupId(1);
      btn->onClick = [this, i] { setVisualMode(i); };
      addAndMakeVisible(*btn);
      visualModeButtons.push_back(std::move(btn));
    }

    // Color theme buttons
    const juce::Colour themeIndicators[] = {
      juce::Colour(0xff32b7ff), // Calm Blue
      juce::Colour(0xffff9a3c), // Warm Sunset
      juce::Colour(0xff3cffaa), // Forest Mint
      juce::Colour(0xffaaaaaa)  // High Contrast
    };
    const char* themeLabels[] = { "Calm Blue", "Warm Sunset", "Forest Mint", "High Contrast" };

    for (int i = 0; i < 4; ++i)
    {
      auto btn = std::make_unique<OptionButton>(themeLabels[i], true, themeIndicators[i]);
      btn->setRadioGroupId(2);
      btn->onClick = [this, i] { setColorTheme(i); };
      addAndMakeVisible(*btn);
      colorThemeButtons.push_back(std::move(btn));
    }

    // Beats per bar slider
    beatsPerBarSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    beatsPerBarSlider.setRange(1, 16, 1);
    beatsPerBarSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 24);
    beatsPerBarSlider.onValueChange = [this] { setBeatsPerBar(static_cast<int>(beatsPerBarSlider.getValue())); };
    addAndMakeVisible(beatsPerBarSlider);

    // Subdivision buttons
    for (int i = 0; i < 4; ++i)
    {
      juce::String label = juce::String(i + 1) + "x";
      auto btn = std::make_unique<OptionButton>(label);
      btn->setRadioGroupId(3);
      btn->onClick = [this, i] { setSubdivisions(i + 1); };
      addAndMakeVisible(*btn);
      subdivisionButtons.push_back(std::move(btn));
    }

    // Volume slider
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.onValueChange = [this] { setVolume(static_cast<float>(volumeSlider.getValue())); };
    addAndMakeVisible(volumeSlider);

    // Close button
    closeButton.setButtonText("Close");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    refreshFromProcessor();
  }

  void setColors(ThemeColors colors)
  {
    theme = colors;

    // keep theme indicators in sync with actual theme palette
    const juce::Colour indicators[] = {
      getThemeColors(ColorTheme::CalmBlue).accent,
      getThemeColors(ColorTheme::WarmSunset).accent,
      getThemeColors(ColorTheme::ForestMint).accent,
      getThemeColors(ColorTheme::HighContrast).accent
    };

    for (size_t i = 0; i < visualModeButtons.size(); ++i)
      visualModeButtons[i]->setTheme(theme);

    for (size_t i = 0; i < colorThemeButtons.size(); ++i)
    {
      colorThemeButtons[i]->setTheme(theme);
      if (i < std::size(indicators))
        colorThemeButtons[i]->setIndicatorColor(indicators[i]);
    }

    for (size_t i = 0; i < subdivisionButtons.size(); ++i)
      subdivisionButtons[i]->setTheme(theme);

    repaint();
  }

  void refreshFromProcessor()
  {
    const auto visualMode = static_cast<int>(processor.getVisualMode());
    const auto colorTheme = static_cast<int>(processor.getColorTheme());
    const auto beatsPerBar = processor.getBeatsPerBar();
    const auto subdivisions = processor.getSubdivisions();
    const auto volume = processor.getSoundVolume();

    for (size_t i = 0; i < visualModeButtons.size(); ++i)
      visualModeButtons[i]->setToggleState(static_cast<int>(i) == visualMode, juce::dontSendNotification);

    for (size_t i = 0; i < colorThemeButtons.size(); ++i)
      colorThemeButtons[i]->setToggleState(static_cast<int>(i) == colorTheme, juce::dontSendNotification);

    beatsPerBarSlider.setValue(beatsPerBar, juce::dontSendNotification);

    for (size_t i = 0; i < subdivisionButtons.size(); ++i)
      subdivisionButtons[i]->setToggleState(static_cast<int>(i) + 1 == subdivisions, juce::dontSendNotification);

    volumeSlider.setValue(volume, juce::dontSendNotification);
  }

  std::function<void()> onClose;

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat();

    // Dark panel background with rounded corners
    g.setColour(theme.panelBg.withAlpha(0.96f));
    g.fillRoundedRectangle(bounds, 16.0f);

    g.setColour(theme.accent.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 16.0f, 1.0f);

    // Title
    g.setColour(theme.textPrimary);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("Settings", 20, 20, 200, 30, juce::Justification::centredLeft);

    // Section labels
    g.setFont(juce::Font(14.0f, juce::Font::plain));
    g.setColour(theme.textMuted.withAlpha(0.95f));
    g.drawText("Visual Mode", 20, 60, 200, 20, juce::Justification::centredLeft);
    g.drawText("Color Theme", 20, 160, 200, 20, juce::Justification::centredLeft);
    g.drawText("Beats Per Bar", 20, 260, 200, 20, juce::Justification::centredLeft);
    g.drawText("Subdivisions (Pattern Mode)", 20, 320, 250, 20, juce::Justification::centredLeft);
    g.drawText("Sound Volume", 20, 400, 200, 20, juce::Justification::centredLeft);
  }

  void resized() override
  {
    auto bounds = getLocalBounds().reduced(20);

    // Close button
    closeButton.setBounds(bounds.getWidth() - 40, 20, 60, 30);

    // Visual Mode buttons (2 rows of 3)
    auto visualModeArea = juce::Rectangle<int>(20, 85, bounds.getWidth() - 20, 60);
    const int btnWidth = (visualModeArea.getWidth() - 20) / 3;
    const int btnHeight = 32;

    for (int i = 0; i < 3; ++i)
      visualModeButtons[static_cast<size_t>(i)]->setBounds(visualModeArea.getX() + i * (btnWidth + 10), visualModeArea.getY(), btnWidth, btnHeight);

    for (int i = 3; i < 6; ++i)
      visualModeButtons[static_cast<size_t>(i)]->setBounds(visualModeArea.getX() + (i - 3) * (btnWidth + 10), visualModeArea.getY() + btnHeight + 5, btnWidth, btnHeight);

    // Color Theme buttons (2 rows of 2)
    auto colorThemeArea = juce::Rectangle<int>(20, 185, bounds.getWidth() - 20, 60);
    const int themeBtnWidth = (colorThemeArea.getWidth() - 10) / 2;

    for (int i = 0; i < 2; ++i)
      colorThemeButtons[static_cast<size_t>(i)]->setBounds(colorThemeArea.getX() + i * (themeBtnWidth + 10), colorThemeArea.getY(), themeBtnWidth, btnHeight);

    for (int i = 2; i < 4; ++i)
      colorThemeButtons[static_cast<size_t>(i)]->setBounds(colorThemeArea.getX() + (i - 2) * (themeBtnWidth + 10), colorThemeArea.getY() + btnHeight + 5, themeBtnWidth, btnHeight);

    // Beats per bar slider
    beatsPerBarSlider.setBounds(20, 285, bounds.getWidth() - 20, 24);

    // Subdivision buttons
    auto subArea = juce::Rectangle<int>(20, 345, bounds.getWidth() - 20, 40);
    const int subBtnWidth = (subArea.getWidth() - 30) / 4;
    for (int i = 0; i < 4; ++i)
      subdivisionButtons[static_cast<size_t>(i)]->setBounds(subArea.getX() + i * (subBtnWidth + 10), subArea.getY(), subBtnWidth, btnHeight);

    // Volume slider
    volumeSlider.setBounds(50, 425, bounds.getWidth() - 70, 24);
  }

private:
  void setVisualMode(int mode)
  {
    if (auto* param = processor.apvts.getParameter(kVisualModeParamId))
      param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(mode)));
  }

  void setColorTheme(int theme)
  {
    if (auto* param = processor.apvts.getParameter(kColorThemeParamId))
      param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(theme)));
  }

  void setBeatsPerBar(int beats)
  {
    if (auto* param = processor.apvts.getParameter(kBeatsPerBarParamId))
      param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(beats)));
  }

  void setSubdivisions(int subs)
  {
    if (auto* param = processor.apvts.getParameter(kSubdivisionsParamId))
      param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(subs)));
  }

  void setVolume(float vol)
  {
    if (auto* param = processor.apvts.getParameter(kSoundVolumeParamId))
      param->setValueNotifyingHost(vol);
  }

  VizBeatsAudioProcessor& processor;

  std::vector<std::unique_ptr<OptionButton>> visualModeButtons;
  std::vector<std::unique_ptr<OptionButton>> colorThemeButtons;
  std::vector<std::unique_ptr<OptionButton>> subdivisionButtons;

  juce::Slider beatsPerBarSlider;
  juce::Slider volumeSlider;
  juce::TextButton closeButton;

  ThemeColors theme = getThemeColors(ColorTheme::HighContrast);
};

//==============================================================================
// Transport Bar
//==============================================================================
class VizBeatsAudioProcessorEditor::TransportBar final : public juce::Component
{
public:
  explicit TransportBar(VizBeatsAudioProcessor& p)
      : processor(p),
        settingsButton(makeGearPath()),
        minusButton("-"),
        plusButton("+")
  {
    addAndMakeVisible(settingsButton);
    addAndMakeVisible(minusButton);
    addAndMakeVisible(plusButton);
    addAndMakeVisible(bpmReadout);
    addAndMakeVisible(playPauseButton);

    settingsButton.setTooltip("Settings");
    minusButton.setTooltip("Decrease manual BPM");
    plusButton.setTooltip("Increase manual BPM");
    playPauseButton.setTooltip("Internal preview play/pause");

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

  std::function<void()> onSettingsClick;

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

  void setColors(ThemeColors colors)
  {
    theme = colors;
    bpmReadout.setColors(colors.textPrimary, colors.textMuted);
    playPauseButton.setAccentColor(colors.accent);
    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    auto bounds = getLocalBounds().toFloat();
    const auto radius = bounds.getHeight() * 0.50f;

    // Subtle unified background - just slightly lighter than main background
    g.setColour(theme.background.brighter(0.08f));
    g.fillRoundedRectangle(bounds, radius);
  }

  void resized() override
  {
    auto bounds = getLocalBounds();
    const auto height = bounds.getHeight();

    // Play button on the right
    auto right = bounds.removeFromRight(height);
    playPauseButton.setBounds(right.reduced(12));

    // Separator space
    bounds.removeFromRight(20);

    // Settings button on the left with proper padding
    bounds.removeFromLeft(16);
    const auto iconSize = static_cast<int>(height * 0.42f);
    const auto iconY = (height - iconSize) / 2;
    settingsButton.setBounds(bounds.removeFromLeft(iconSize).withY(iconY).withHeight(iconSize));
    settingsButton.onClick = [this] { if (onSettingsClick) onSettingsClick(); };

    // Space after settings
    bounds.removeFromLeft(24);

    // Center area for BPM controls
    auto centre = bounds.reduced(0, 16);

    // - and + buttons with better sizing
    const auto stepW = juce::jmin(80, static_cast<int>(centre.getWidth() * 0.20f));
    const auto stepH = centre.getHeight() - 16;

    minusButton.setBounds(centre.removeFromLeft(stepW).withSizeKeepingCentre(stepW, stepH));
    centre.removeFromLeft(16);
    plusButton.setBounds(centre.removeFromRight(stepW).withSizeKeepingCentre(stepW, stepH));
    centre.removeFromRight(16);

    // BPM readout in the center
    bpmReadout.setBounds(centre);
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

  IconButton settingsButton;
  StepButton minusButton;
  StepButton plusButton;
  BpmReadout bpmReadout;
  PlayPauseButton playPauseButton;

  bool hostPlaying = false;
  ThemeColors theme = getThemeColors(ColorTheme::HighContrast);
};

//==============================================================================
// Main Editor
//==============================================================================
VizBeatsAudioProcessorEditor::VizBeatsAudioProcessorEditor(VizBeatsAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
  setOpaque(true);

  pulseVisualizer = std::make_unique<PulseVisualizer>();
  trafficVisualizer = std::make_unique<TrafficVisualizer>();
  transportBar = std::make_unique<TransportBar>(processor);
  settingsPanel = std::make_unique<SettingsPanel>(processor);

  addAndMakeVisible(*pulseVisualizer);
  addAndMakeVisible(*trafficVisualizer);
  addAndMakeVisible(*transportBar);
  addChildComponent(*settingsPanel);

  transportBar->onSettingsClick = [this]
  {
    settingsVisible = !settingsVisible;
    settingsPanel->setVisible(settingsVisible);
    if (settingsVisible)
      settingsPanel->refreshFromProcessor();
    resized();
  };

  settingsPanel->onClose = [this]
  {
    settingsVisible = false;
    settingsPanel->setVisible(false);
    resized();
  };

  updateVisualizerVisibility();

  setSize(960, 540);
  startTimerHz(60);
}

VizBeatsAudioProcessorEditor::~VizBeatsAudioProcessorEditor()
{
  stopTimer();
}

void VizBeatsAudioProcessorEditor::updateVisualizerVisibility()
{
  const auto mode = processor.getVisualMode();
  pulseVisualizer->setVisible(mode == VisualMode::Pulse);
  trafficVisualizer->setVisible(mode == VisualMode::Traffic);
}

void VizBeatsAudioProcessorEditor::paint(juce::Graphics& g)
{
  const auto theme = getThemeColors(processor.getColorTheme());
  auto bounds = getLocalBounds().toFloat();

  // Uniform background - single color, no gradients
  g.setColour(theme.background);
  g.fillRect(bounds);

  if (settingsVisible)
  {
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(bounds);
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

  // Make visualizers cover the full area above the transport bar
  // They will handle their own internal margins
  auto vizBounds = bounds.withTrimmedBottom(barHeight + 18);
  if (pulseVisualizer != nullptr)
    pulseVisualizer->setBounds(vizBounds);

  if (trafficVisualizer != nullptr)
    trafficVisualizer->setBounds(vizBounds);

  // Settings panel - centered
  if (settingsPanel != nullptr)
  {
    const auto panelWidth = juce::jmin(450, bounds.getWidth() - 40);
    const auto panelHeight = 480;
    const auto panelX = (bounds.getWidth() - panelWidth) / 2;
    const auto panelY = (bounds.getHeight() - panelHeight) / 2;
    settingsPanel->setBounds(panelX, panelY, panelWidth, panelHeight);
  }
}

void VizBeatsAudioProcessorEditor::timerCallback()
{
  processor.refreshHostInfo();

  const auto hostInfo = processor.getHostInfo();
  const auto manualBpm = static_cast<double>(processor.apvts.getRawParameterValue(kManualBpmParamId)->load());
  const auto internalPlay = processor.apvts.getRawParameterValue(kInternalPlayParamId)->load() > 0.5f;
  const auto beatsPerBar = processor.getBeatsPerBar();
  const auto subdivisions = processor.getSubdivisions();
  const auto currentTheme = processor.getColorTheme();
  const auto activeTheme = getThemeColors(currentTheme);

  // Check if theme changed and repaint entire editor
  if (currentTheme != lastColorTheme)
  {
    lastColorTheme = currentTheme;
    repaint(); // Repaint the entire editor
  }

  // Track when internal play or host play starts for timing fallback
  if ((internalPlay && !lastInternalPlayState) || (hostInfo.isPlaying && !lastHostPlayingState))
    internalStartTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

  lastInternalPlayState = internalPlay;
  lastHostPlayingState = hostInfo.isPlaying;

  const auto hostPlaying = hostInfo.isPlaying;

  // Always prefer host BPM when available, regardless of host playing state
  // This shows the project tempo even when stopped
  const auto effectiveBpm = hostInfo.hasBpm ? hostInfo.bpm : manualBpm;

  // When host is playing, override internal play state for display
  transportBar->setHostPlaying(hostPlaying);
  transportBar->setPlayState(hostPlaying || internalPlay);
  transportBar->setBpm(effectiveBpm);
  transportBar->setColors(activeTheme);
  if (settingsPanel != nullptr)
    settingsPanel->setColors(activeTheme);

  bool isRunning = false;
  double beatPhase = 0.0;

  // Host transport takes priority - when DAW is playing, sync to it
  if (hostPlaying)
  {
    isRunning = true;

    if (hostInfo.hasPpqPosition)
    {
      const auto ppq = hostInfo.ppqPosition;
      beatPhase = ppq - std::floor(ppq);
      if (beatPhase < 0.0)
        beatPhase += 1.0;

      // Calculate current beat in bar from PPQ
      currentBeatInBar = static_cast<int>(std::fmod(ppq, static_cast<double>(beatsPerBar)));
    }
    else
    {
      // Host playing but no PPQ - use time-based fallback with host BPM
      const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
      const auto elapsedSeconds = juce::jmax(0.0, nowSeconds - internalStartTimeSeconds);
      const auto useBpm = hostInfo.hasBpm ? hostInfo.bpm : effectiveBpm;
      const auto beats = elapsedSeconds * (useBpm / 60.0);
      beatPhase = beats - std::floor(beats);
      currentBeatInBar = static_cast<int>(std::fmod(beats, static_cast<double>(beatsPerBar)));
    }
  }
  else if (internalPlay)
  {
    // Only use internal play when host is NOT playing
    isRunning = true;
    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsedSeconds = juce::jmax(0.0, nowSeconds - internalStartTimeSeconds);
    // Use project BPM if the host provides it even while stopped; fall back to manual BPM.
    const auto beats = elapsedSeconds * (effectiveBpm / 60.0);
    beatPhase = beats - std::floor(beats);

    // Calculate current beat in bar
    currentBeatInBar = static_cast<int>(std::fmod(beats, static_cast<double>(beatsPerBar)));
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
    currentBeatInBar = 0;
  }
  lastUiRunning = isRunning;

  // Update visualizer visibility based on mode
  updateVisualizerVisibility();

  // Update pulse visualizer
  pulseVisualizer->setRunning(isRunning);
  pulseVisualizer->setPulse(isRunning ? (beatWrapped ? 1.0f : pulseFromBeatPhase(beatPhase)) : 0.0f);
  pulseVisualizer->setColors(activeTheme);

  // Update traffic visualizer
  trafficVisualizer->setRunning(isRunning);
  trafficVisualizer->setBeatPhase(beatPhase);
  trafficVisualizer->setBeatsPerBar(beatsPerBar);
  trafficVisualizer->setSubdivisions(subdivisions);
  trafficVisualizer->setCurrentBeat(currentBeatInBar);
  trafficVisualizer->setColors(activeTheme);

  pulseVisualizer->repaint();
  trafficVisualizer->repaint();
  transportBar->repaint();
}
