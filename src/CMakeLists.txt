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

juce_generate_juce_header(EffectBox)

target_sources(EffectBox PRIVATE
	plugins/ARAPlugin.cpp
	plugins/IOConfigurationWindow.cpp
	plugins/InternalPlugins.cpp
	plugins/PluginGraph.cpp

	ui/GraphEditorPanel.cpp
	ui/MainHostWindow.cpp

	App.cpp
)

target_include_directories(EffectBox PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(EffectBox PRIVATE
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
	JUCE_PLUGINHOST_ARA=0
	JUCE_VST3_HOST_CROSS_PLATFORM_UID=1

	JUCE_USE_CAMERA=0
	JUCE_USE_CDBURNER=0
	JUCE_USE_CDREADER=0
	JUCE_USE_CURL=0
	JUCE_USE_FLAC=0
	JUCE_USE_OGGVORBIS=0
	JUCE_WEB_BROWSER=0
)

target_link_libraries(EffectBox PRIVATE
	juce::juce_audio_utils
	juce::juce_cryptography
	juce::juce_dsp
	juce::juce_opengl
	juce::juce_recommended_config_flags
	juce::juce_recommended_lto_flags
	juce::juce_recommended_warning_flags
)
