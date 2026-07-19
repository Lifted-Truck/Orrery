# shell/plugin — the JUCE studio shell (JUCE lives here and ONLY here; the
# core-boundary ctest fails the build if JUCE leaks into shell/core).
#
# Wraps orrery_core in a VST3/AU/Standalone instrument. Built only when
# ORRERY_BUILD_PLUGIN=ON (so the core `fast` gate stays offline + quick).

option(ORRERY_COPY_PLUGIN "Install the plugin to the user plugin folder after build" OFF)

juce_add_plugin(Orrery
    COMPANY_NAME              "Lifted Truck"
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

target_sources(Orrery PRIVATE
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

target_compile_definitions(Orrery PRIVATE
    ORRERY_WITH_EDITOR=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_include_directories(Orrery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engines)   # engine views: <name>/gui/...

target_link_libraries(Orrery
    PRIVATE
        orrery_elastic
        orrery_measured
        orrery_probable
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Codesign re-seal AFTER build (global CLAUDE.md gotcha): JUCE regenerates
# moduleinfo.json after its own ad-hoc signing, breaking the seal — a bundle
# with a broken seal is SILENTLY skipped by the DAW scanner. Re-sign every
# format bundle so each build is loadable. (Run the build with the sandbox
# disabled so codesign hits real disk, per the same gotcha.)
if(APPLE)
    foreach(fmt VST3 AU Standalone)
        if(TARGET Orrery_${fmt})
            add_custom_command(TARGET Orrery_${fmt} POST_BUILD
                COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:Orrery_${fmt}>"
                COMMENT "codesign re-seal Orrery_${fmt}"
                VERBATIM)
        endif()
    endforeach()
endif()

# ── O1b RT-safety gate: headless console app driving renderBlock ─────────────
juce_add_console_app(orrery_rt_test PRODUCT_NAME orrery_rt_test)
target_sources(orrery_rt_test PRIVATE
    tests/test_rt_noalloc.cpp
    PluginProcessor.cpp
    EngineSlot.cpp
    TraceDrain.cpp
    MidiOut.cpp)
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
