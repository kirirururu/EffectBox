juce_add_gui_app(EffectBoxEngine
	VERSION 0.1.0
	BUNDLE_ID com.kiriru.effectbox.engine
	ICON_BIG "${CMAKE_SOURCE_DIR}/resources/JUCEAppIcon.png"
	ICON_COMPOSER_BUNDLE "${CMAKE_SOURCE_DIR}/resources/AppIcon.icon"
	MICROPHONE_PERMISSION_ENABLED TRUE
	PLUGINHOST_AU FALSE
	NEEDS_WEB_BROWSER FALSE
	NEEDS_CURL FALSE
)

set_target_properties(EffectBoxEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

target_sources(EffectBoxEngine PRIVATE
	SineWaveSynthPlugin.cpp
	ReverbPlugin.cpp
	InternalPlugins.cpp
	PluginGraph.cpp
	PluginWindow.cpp
	PluginDebugWindow.cpp
	CustomPluginScanner.cpp
	EngineServer.cpp
	EngineApp.cpp
)

target_compile_definitions(EffectBoxEngine PRIVATE
	JUCE_ALSA=0
	JUCE_PIPEWIRE=0
	JUCE_PULSEAUDIO=0
	JUCE_JACK=1
	JUCE_DIRECTSOUND=0
	JUCE_WASAPI=0
	JUCE_JACK_CLIENT_NAME="EffectBox"

	JUCE_CONTENT_SHARING=1
	JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1

	JUCE_PLUGINHOST_LADSPA=1
	JUCE_PLUGINHOST_LV2=1
	JUCE_PLUGINHOST_VST3=1
	JUCE_PLUGINHOST_VST=0
	JUCE_VST3_HOST_CROSS_PLATFORM_UID=1

	JUCE_USE_CAMERA=0
	JUCE_USE_CDBURNER=0
	JUCE_USE_CDREADER=0
	JUCE_USE_CURL=0
	JUCE_USE_FLAC=0
	JUCE_USE_OGGVORBIS=0
	JUCE_WEB_BROWSER=0
)

target_link_libraries(EffectBoxEngine PRIVATE
	EffectBoxCommon
	juce::juce_audio_utils
	juce::juce_recommended_config_flags
	juce::juce_recommended_lto_flags
	juce::juce_recommended_warning_flags
)
