# shell/plugin — the JUCE studio shell (JUCE lives here and ONLY here; the
# core-boundary ctest fails the build if JUCE leaks into shell/core).
#
# Wraps orrery_core in a VST3/AU/Standalone instrument. Built only when
# ORRERY_BUILD_PLUGIN=ON (so the core `fast` gate stays offline + quick).

option(ORRERY_COPY_PLUGIN "Install the plugin to the user plugin folder after build" OFF)

# Shared shell sources — compiled into every plugin variant + the RT test.
set(ORRERY_SHELL_SOURCES
    PluginProcessor.cpp
    EngineSlot.cpp
    TraceDrain.cpp
    MidiOut.cpp
    gui/OrreryLookAndFeel.cpp
    gui/Chrome.cpp
    gui/ParamRail.cpp
    gui/OffsetLane.cpp
    gui/PluginEditor.cpp
    # Engine views live in their engine's territory (engines/<name>/gui/ — the
    # organ owns its visualization); the plugin target is where they COMPILE,
    # since only this target has JUCE. Add one line per engine view.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engines/elastic-euclid/gui/ElasticView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engines/measured-euclid/gui/MeasuredView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engines/probable-euclid/gui/ProbableView.cpp)

# Apply the shared sources / defs / links / codesign-reseal to a plugin target.
# Two variants share this: the instrument (Orrery) and the MIDI-effect
# (OrreryMFX). Same code; JucePlugin_IsMidiEffect (set by juce_add_plugin)
# switches the processor's bus config + isMidiEffect() at compile time.
function(orrery_configure_plugin tgt)
    target_sources(${tgt} PRIVATE ${ORRERY_SHELL_SOURCES})
    target_compile_definitions(${tgt} PRIVATE
        ORRERY_WITH_EDITOR=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../../engines)
    target_link_libraries(${tgt}
        PRIVATE orrery_elastic orrery_measured orrery_probable juce::juce_audio_utils
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    # Codesign re-seal AFTER build (global CLAUDE.md gotcha): JUCE regenerates
    # moduleinfo.json after its own ad-hoc signing, breaking the seal — a bundle
    # with a broken seal is SILENTLY skipped by the DAW scanner.
    if(APPLE)
        foreach(fmt VST3 AU Standalone)
            if(TARGET ${tgt}_${fmt})
                add_custom_command(TARGET ${tgt}_${fmt} POST_BUILD
                    COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:${tgt}_${fmt}>"
                    COMMENT "codesign re-seal ${tgt}_${fmt}" VERBATIM)
            endif()
        endforeach()
    endif()
endfunction()

# ── Instrument: internal voices + MIDI-out (cross-track routing / virtual port).
juce_add_plugin(Orrery
    COMPANY_NAME              "Mindlathe"
    # BUNDLE_ID pinned to the value JUCE previously derived from COMPANY_NAME, so the
    # vendor rename (2026-09-27) changes only what hosts DISPLAY. Identity lives in
    # PLUGIN_MANUFACTURER_CODE + PLUGIN_CODE (the VST3 class ID and AU codes); never edit those.
    BUNDLE_ID                 "com.Lifted Truck.Orrery"
    PLUGIN_MANUFACTURER_CODE  Lftk
    PLUGIN_CODE               Orry
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Orrery"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   ${ORRERY_COPY_PLUGIN}
    VST3_CATEGORIES           Instrument)
orrery_configure_plugin(Orrery)

# ── MIDI effect: drops BEFORE an instrument in the same track (Ableton 11.1+).
# No audio output — the downstream instrument sounds; the internal voices and
# the virtual CoreMIDI port are inert here (feeds the in-track instrument
# directly). Distinct PLUGIN_CODE so hosts see it as a separate device.
juce_add_plugin(OrreryMFX
    COMPANY_NAME              "Mindlathe"
    # BUNDLE_ID pinned to the value JUCE previously derived from COMPANY_NAME, so the
    # vendor rename (2026-09-27) changes only what hosts DISPLAY. Identity lives in
    # PLUGIN_MANUFACTURER_CODE + PLUGIN_CODE (the VST3 class ID and AU codes); never edit those.
    BUNDLE_ID                 "com.Lifted Truck.OrreryMFX"
    PLUGIN_MANUFACTURER_CODE  Lftk
    PLUGIN_CODE               OrrM
    FORMATS                   VST3 AU
    PRODUCT_NAME              "Orrery MFX"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            TRUE
    COPY_PLUGIN_AFTER_BUILD   ${ORRERY_COPY_PLUGIN})
orrery_configure_plugin(OrreryMFX)

# ── O1b RT-safety gate: headless console app driving renderBlock ─────────────
juce_add_console_app(orrery_rt_test PRODUCT_NAME orrery_rt_test)
target_sources(orrery_rt_test PRIVATE
    tests/test_rt_noalloc.cpp
    PluginProcessor.cpp
    EngineSlot.cpp
    TraceDrain.cpp
    MidiOut.cpp)   # not the GUI/views — the RT test is headless (instrument path)
target_compile_definitions(orrery_rt_test PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
target_include_directories(orrery_rt_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(orrery_rt_test
    PRIVATE
        orrery_elastic
        orrery_measured
        orrery_probable
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
add_test(NAME test_rt_noalloc COMMAND orrery_rt_test)
