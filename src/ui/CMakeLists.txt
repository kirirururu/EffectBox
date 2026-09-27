juce_add_gui_app(EffectBox
	VERSION 0.1.0
	BUNDLE_ID com.kiriru.effectbox
	ICON_BIG "${CMAKE_SOURCE_DIR}/resources/JUCEAppIcon.png"
	ICON_COMPOSER_BUNDLE "${CMAKE_SOURCE_DIR}/resources/AppIcon.icon"
	MICROPHONE_PERMISSION_ENABLED TRUE
	PLUGINHOST_AU FALSE
	NEEDS_WEB_BROWSER FALSE
	NEEDS_CURL FALSE
)

set_target_properties(EffectBox PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_sources(EffectBox PRIVATE
	EngineClient.cpp
	GraphEditorPanel.cpp
	GraphIOEditor.cpp
	IOPanels.cpp
	MainHostWindow.cpp
	GuiApp.cpp
)

target_compile_definitions(EffectBox PRIVATE
	JUCE_ALSA=0
	JUCE_PIPEWIRE=0
	JUCE_PULSEAUDIO=0
	JUCE_JACK=0
	JUCE_DIRECTSOUND=0
	JUCE_WASAPI=0

	JUCE_CONTENT_SHARING=1

	JUCE_USE_CAMERA=0
	JUCE_USE_CDBURNER=0
	JUCE_USE_CDREADER=0
	JUCE_USE_CURL=0
	JUCE_USE_FLAC=0
	JUCE_USE_OGGVORBIS=0
	JUCE_WEB_BROWSER=0
)

target_link_libraries(EffectBox PRIVATE
	EffectBoxCommon
	juce::juce_audio_utils
	juce::juce_recommended_config_flags
	juce::juce_recommended_lto_flags
	juce::juce_recommended_warning_flags
)

add_dependencies(EffectBox EffectBoxEngine)
