/************************************************************************************

Filename    :   Oculus360Videos.h
Content     :   Panorama viewer based on SwipeView
Created     :   February 14, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#ifndef VrPanoVideos_H
#define VrPanoVideos_H

#include "VRMenu/Fader.h"
#include "ModelView.h"

namespace OVR {

class OvrVideosMetaData;
class OvrMetaData;
struct OvrMetaDatum;
class VideoBrowser;
class OvrVideoMenu;

enum Action
{
	ACT_NONE,
	ACT_LAUNCHER,
	ACT_STILLS,
	ACT_VIDEOS,
};

class Oculus360Videos : public OVR::VrAppInterface
{
public:

	enum OvrMenuState
	{
		MENU_NONE,
		MENU_BROWSER,
		MENU_VIDEO_LOADING,
		MENU_VIDEO_READY,
		MENU_VIDEO_PLAYING,
		NUM_MENU_STATES
	};

	Oculus360Videos();
	~Oculus360Videos();

	virtual void		OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI );
	virtual void		OneTimeShutdown();
	virtual void		ConfigureVrMode( ovrModeParms & modeParms );
	virtual Matrix4f 	DrawEyeView( const int eye, const float fovDegrees );
	virtual Matrix4f 	Frame( VrFrame vrFrame );
	virtual void		Command( const char * msg );
	virtual bool 		OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType );

	void 				StopVideo();
	void 				PauseVideo( bool const force );
	void 				ResumeVideo();
	bool				IsVideoPlaying() const;

	void 				StartVideo( const double nowTime );
	void				SeekTo( const int seekPos );
	void				SeekToRelative( const int seekRelativePos );
	int					GetCurrentPosition() const;
	int					GetDuration() const;

	void 				CloseSwipeView( const VrFrame &vrFrame );
	void 				OpenSwipeView( const VrFrame &vrFrame, bool centerList );
	Matrix4f			TexmForVideo( const int eye );
	Matrix4f			TexmForBackground( const int eye );

	void				SetMenuState( const OvrMenuState state );
	OvrMenuState		GetCurrentState() const				{ return  MenuState; }

	void				SetFrameAvailable( bool const a ) { FrameAvailable = a; }

	void				OnVideoActivated( const OvrMetaDatum * videoData );
	const OvrMetaDatum * GetActiveVideo()	{ return ActiveVideo;  }
	float				GetFadeLevel()		{ return CurrentFadeLevel; }

private:
	const char*			MenuStateString( const OvrMenuState state );

	// shared vars
	jclass				MainActivityClass;	// need to look up from main thread
	GlGeometry			Globe;
	OvrSceneView		Scene;
	ModelFile *			BackgroundScene;
	bool				VideoWasPlayingWhenPaused;	// state of video when main activity was paused

	// panorama vars
	GLuint				BackgroundTexId;
	GlProgram			PanoramaProgram;
	GlProgram			FadedPanoramaProgram;
	GlProgram			SingleColorTextureProgram;

	Array< String > 	SearchPaths;
	OvrVideosMetaData *	MetaData;
	VideoBrowser *		Browser;
	OvrVideoMenu *		VideoMenu;
	const OvrMetaDatum * ActiveVideo;
	OvrMenuState		MenuState;
	SineFader			Fader;
	const float			FadeOutRate;
	const float			VideoMenuVisibleTime;
	float				CurrentFadeRate;
	float				CurrentFadeLevel;	
	float				VideoMenuTimeLeft;

	bool				UseSrgb;

	// video vars
	String				VideoName;
	SurfaceTexture	* 	MovieTexture;

	// Set when MediaPlayer knows what the stream size is.
	// current is the aspect size, texture may be twice as wide or high for 3D content.
	int					CurrentVideoWidth;	// set to 0 when a new movie is started, don't render until non-0
	int					CurrentVideoHeight;

	int					BackgroundWidth;
	int					BackgroundHeight;

	bool				FrameAvailable;

private:
	void				OnResume();
	void				OnPause();
};

}

#endif	// SWIPELAUNCH_H
