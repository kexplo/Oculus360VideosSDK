/************************************************************************************

Filename    :   Oculus360Videos.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <jni.h>
#include <android/keycodes.h>

#include "Kernel/OVR_Math.h"
#include "Kernel/OVR_TypesafeNumber.h"
#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_String_Utils.h"
#include "Android/GlUtils.h"

#include "GlTexture.h"
#include "BitmapFont.h"
#include "GazeCursor.h"
#include "App.h"
#include "SwipeView.h"
#include "Oculus360Videos.h"
#include "VRMenu/GuiSys.h"
#include "ImageData.h"
#include "PackageFiles.h"
#include "VRMenu/Fader.h"
#include "3rdParty/stb/stb_image.h"
#include "3rdParty/stb/stb_image_write.h"
#include "VrCommon.h"

#include "VideoBrowser.h"
#include "VideoMenu.h"
#include "VrLocale.h"
#include "PathUtils.h"

#include "VideosMetaData.h"

static bool	RetailMode = false;

static const char * videosDirectory = "Oculus/360Videos/";
static const char * videosLabel = "@string/app_name";
static const float	FadeOutTime = 0.25f;
static const float	FadeOverTime = 1.0f;

extern "C" {

static jclass	GlobalActivityClass;
long Java_com_oculus_oculus360videossdk_MainActivity_nativeSetAppInterface( JNIEnv *jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	// This is called by the java UI thread.

	GlobalActivityClass = (jclass)jni->NewGlobalRef( clazz );

	LOG( "nativeSetAppInterface");
	return (new Oculus360Videos())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeFrameAvailable( JNIEnv *jni, jclass clazz, jlong interfacePtr ) {
	Oculus360Videos * panoVids = ( Oculus360Videos * )( ( ( App * )interfacePtr )->GetAppInterface() );
	panoVids->SetFrameAvailable( true );
}

jobject Java_com_oculus_oculus360videossdk_MainActivity_nativePrepareNewVideo( JNIEnv *jni, jclass clazz, jlong interfacePtr ) {

	// set up a message queue to get the return message
	// TODO: make a class that encapsulates this work
	MessageQueue	result( 1 );
	Oculus360Videos * panoVids = ( Oculus360Videos * )( ( ( App * )interfacePtr )->GetAppInterface() );
	panoVids->app->GetMessageQueue().PostPrintf( "newVideo %p", &result );

	result.SleepUntilMessage();
	const char * msg = result.GetNextMessage();
	jobject	texobj;
	sscanf( msg, "surfaceTexture %p", &texobj );
	free( ( void * )msg );

	return texobj;
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeSetVideoSize( JNIEnv *jni, jclass clazz, jlong interfacePtr, int width, int height ) {
	LOG( "nativeSetVideoSizes: width=%i height=%i", width, height );

	Oculus360Videos * panoVids = ( Oculus360Videos * )( ( ( App * )interfacePtr )->GetAppInterface() );
	panoVids->app->GetMessageQueue().PostPrintf( "video %i %i", width, height );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeVideoCompletion( JNIEnv *jni, jclass clazz, jlong interfacePtr ) {
	LOG( "nativeVideoCompletion" );

	Oculus360Videos * panoVids = ( Oculus360Videos * )( ( ( App * )interfacePtr )->GetAppInterface() );
	panoVids->app->GetMessageQueue().PostPrintf( "completion" );
}

void Java_com_oculus_oculus360videossdk_MainActivity_nativeVideoStartError( JNIEnv *jni, jclass clazz, jlong interfacePtr ) {
	LOG( "nativeVideoStartError" );

	Oculus360Videos * panoVids = ( Oculus360Videos * )( ( ( App * )interfacePtr )->GetAppInterface() );
	panoVids->app->GetMessageQueue().PostPrintf( "startError" );
}

} // extern "C"

namespace OVR
{

Oculus360Videos::Oculus360Videos()
	: MainActivityClass( GlobalActivityClass )
	, BackgroundScene( NULL )
	, VideoWasPlayingWhenPaused( false )
	, BackgroundTexId( 0 )
	, MetaData( NULL )
	, Browser( NULL )
	, VideoMenu( NULL )
	, ActiveVideo( NULL )
	, MenuState( MENU_NONE )
	, Fader( 1.0f )
	, FadeOutRate( 1.0f / 0.5f )
	, VideoMenuVisibleTime( 5.0f )
	, CurrentFadeRate( FadeOutRate )
	, CurrentFadeLevel( 1.0f )
	, VideoMenuTimeLeft( 0.0f )
	, UseSrgb( false )
	, MovieTexture( NULL )
	, CurrentVideoWidth( 0 )
	, CurrentVideoHeight( 480 )
	, BackgroundWidth( 0 )
	, BackgroundHeight( 0 )
	, FrameAvailable( false )
{
}

Oculus360Videos::~Oculus360Videos()
{
}

//============================================================================================
void Oculus360Videos::OneTimeInit( const char * fromPackage, const char * launchIntentJSON, const char * launchIntentURI )
{
	// This is called by the VR thread, not the java UI thread.
	LOG( "--------------- Oculus360Videos OneTimeInit ---------------" );

	RetailMode = FileExists( "/sdcard/RetailMedia" );

	app->GetVrParms().colorFormat = COLOR_8888;
	app->GetVrParms().depthFormat = DEPTH_16;
	app->GetVrParms().multisamples = 2;

	PanoramaProgram = BuildProgram(
		"uniform highp mat4 Mvpm;\n"
		"uniform highp mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"varying  highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = vec2( Texm * vec4( TexCoord, 0, 1 ) );\n"
		"}\n"
		,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform samplerExternalOES Texture0;\n"
		"uniform lowp vec4 UniformColor;\n"
		"uniform lowp vec4 ColorBias;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = ColorBias + UniformColor * texture2D( Texture0, oTexCoord );\n"
		"}\n"
		);

	FadedPanoramaProgram = BuildProgram(
		"uniform highp mat4 Mvpm;\n"
		"uniform highp mat4 Texm;\n"
		"attribute vec4 Position;\n"
		"attribute vec2 TexCoord;\n"
		"varying  highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = vec2( Texm * vec4( TexCoord, 0, 1 ) );\n"
		"}\n"
		,
		"#extension GL_OES_EGL_image_external : require\n"
		"uniform samplerExternalOES Texture0;\n"
		"uniform sampler2D Texture1;\n"
		"uniform lowp vec4 UniformColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"	lowp vec4 staticColor = texture2D( Texture1, oTexCoord );\n"
		"	lowp vec4 movieColor = texture2D( Texture0, oTexCoord );\n"
		"	gl_FragColor = UniformColor * mix( movieColor, staticColor, staticColor.w );\n"
		"}\n"
		);

	SingleColorTextureProgram = BuildProgram(
		"uniform highp mat4 Mvpm;\n"
		"attribute highp vec4 Position;\n"
		"attribute highp vec2 TexCoord;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = Mvpm * Position;\n"
		"   oTexCoord = TexCoord;\n"
		"}\n"
		,
		"uniform sampler2D Texture0;\n"
		"uniform lowp vec4 UniformColor;\n"
		"varying highp vec2 oTexCoord;\n"
		"void main()\n"
		"{\n"
		"   gl_FragColor = UniformColor * texture2D( Texture0, oTexCoord );\n"
		"}\n"
		);

	const char *launchPano = NULL;
	if ( ( NULL != launchPano ) && launchPano[ 0 ] )
	{
#if 0
		unsigned char * texture;
		texture = stbi_load( launchPano, &BackgroundWidth, &BackgroundHeight, NULL, 4 );
		if ( texture )
		{
			for ( int i = 0; i < BackgroundWidth*BackgroundHeight; i++ )
			{
				texture[ i * 4 + 3 ] = 255;
			}

			// cut a hole in the middle
			for ( int i = 0; i < BackgroundWidth / 2; i++ )
			for ( int j = 0; j < BackgroundHeight / 4; j++ )
			{
				texture[ ( ( BackgroundHeight / 8 + j )*BackgroundWidth + BackgroundWidth / 4 + 256 + i ) * 4 + 3 ] = 0;
				texture[ ( ( 5 * BackgroundHeight / 8 + j )*BackgroundWidth + BackgroundWidth / 4 + 256 + i ) * 4 + 3 ] = 0;
			}

			BackgroundTexId = LoadRGBATextureFromMemory( texture, BackgroundWidth, BackgroundHeight, false );
		}
#else
		BackgroundTexId = LoadTextureFromBuffer( launchPano, MemBufferFile( launchPano ),
			TextureFlags_t( TEXTUREFLAG_NO_DEFAULT ) | TEXTUREFLAG_USE_SRGB, BackgroundWidth, BackgroundHeight );
#endif
	}

	// always fall back to valid background
	if ( BackgroundTexId == 0 )
	{
		BackgroundTexId = LoadTextureFromApplicationPackage( "assets/background.jpg",
			TextureFlags_t( TEXTUREFLAG_USE_SRGB ), BackgroundWidth, BackgroundHeight );
	}

	LOG( "Creating Globe" );
	Globe = BuildGlobe();

	// Stay exactly at the origin, so the panorama globe is equidistant
	// Don't clear the head model neck length, or swipe view panels feel wrong.
	VrViewParms viewParms = app->GetVrViewParms();
	viewParms.EyeHeight = 0.0f;
	app->SetVrViewParms( viewParms );

	// Optimize for 16 bit depth in a modest theater size
	Scene.Znear = 0.1f;
	Scene.Zfar = 200.0f;
	MaterialParms materialParms;
	materialParms.UseSrgbTextureFormats = ( app->GetVrParms().colorFormat == COLOR_8888_sRGB );

	BackgroundScene = LoadModelFileFromApplicationPackage( "assets/stars.ovrscene",
		Scene.GetDefaultGLPrograms(),
		materialParms );

	Scene.SetWorldModel( *BackgroundScene );

	// Load up meta data from videos directory
	MetaData = new OvrVideosMetaData();
	if ( MetaData == NULL )
	{
		FAIL( "Oculus360Photos::OneTimeInit failed to create MetaData" );
	}

	const OvrStoragePaths & storagePaths = app->GetStoragePaths();
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_SECONDARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "RetailMedia/", SearchPaths );
	storagePaths.PushBackSearchPathIfValid( EST_PRIMARY_EXTERNAL_STORAGE, EFT_ROOT, "", SearchPaths );

	OvrMetaDataFileExtensions fileExtensions;
	fileExtensions.GoodExtensions.PushBack( ".mp4" );
	fileExtensions.GoodExtensions.PushBack( ".m4v" );
	fileExtensions.GoodExtensions.PushBack( ".3gp" );
	fileExtensions.GoodExtensions.PushBack( ".3g2" );
	fileExtensions.GoodExtensions.PushBack( ".ts" );
	fileExtensions.GoodExtensions.PushBack( ".webm" );
	fileExtensions.GoodExtensions.PushBack( ".mkv" );
	fileExtensions.GoodExtensions.PushBack( ".wmv" );
	fileExtensions.GoodExtensions.PushBack( ".asf" );
	fileExtensions.GoodExtensions.PushBack( ".avi" );
	fileExtensions.GoodExtensions.PushBack( ".flv" );

	MetaData->InitFromDirectory( videosDirectory, SearchPaths, fileExtensions );

	String localizedAppName;
	VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), videosLabel, videosLabel, localizedAppName );
	MetaData->RenameCategory( ExtractFileBase( videosDirectory ), localizedAppName );

	// Start building the VideoMenu
	VideoMenu = ( OvrVideoMenu * )app->GetGuiSys().GetMenu( OvrVideoMenu::MENU_NAME );
	if ( VideoMenu == NULL )
	{
		VideoMenu = OvrVideoMenu::Create(
			app, this, app->GetVRMenuMgr(), app->GetDefaultFont(), *MetaData, 1.0f, 2.0f );
		OVR_ASSERT( VideoMenu );

		app->GetGuiSys().AddMenu( VideoMenu );
	}

	VideoMenu->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_SHORT_PRESS_HANDLED_BY_APP );

	// Start building the FolderView
	Browser = ( VideoBrowser * )app->GetGuiSys().GetMenu( OvrFolderBrowser::MENU_NAME );
	if ( Browser == NULL )
	{
		Browser = VideoBrowser::Create(
			app,
			*MetaData,
			256, 20.0f,
			256, 200.0f,
			7,
			5.4f );
		OVR_ASSERT( Browser );

		app->GetGuiSys().AddMenu( Browser );
	}

	Browser->SetFlags( VRMenuFlags_t( VRMENU_FLAG_PLACE_ON_HORIZON ) | VRMENU_FLAG_BACK_KEY_EXITS_APP );
	Browser->SetFolderTitleSpacingScale( 0.37f );
	Browser->SetPanelTextSpacingScale( 0.34f );
	Browser->SetScrollBarSpacingScale( 0.9f );
	Browser->SetScrollBarRadiusScale( 1.0f );

	Browser->OneTimeInit();
	Browser->BuildDirtyMenu( *MetaData );

	SetMenuState( MENU_BROWSER );
}

void Oculus360Videos::OneTimeShutdown()
{
	// This is called by the VR thread, not the java UI thread.
	LOG( "--------------- Oculus360Videos OneTimeShutdown ---------------" );

	if ( BackgroundScene != NULL )
	{
		delete BackgroundScene;
		BackgroundScene = NULL;
	}

	if ( MetaData != NULL )
	{
		delete MetaData;
		MetaData = NULL;
	}

	Globe.Free();

	FreeTexture( BackgroundTexId );

	if ( MovieTexture != NULL )
	{
		delete MovieTexture;
		MovieTexture = NULL;
	}

	DeleteProgram( PanoramaProgram );
	DeleteProgram( FadedPanoramaProgram );
	DeleteProgram( SingleColorTextureProgram );
}

void Oculus360Videos::ConfigureVrMode( ovrModeParms & modeParms )
{
	// We need very little CPU for pano browsing, but a fair amount of GPU.
	// The CPU clock should ramp up above the minimum when necessary.
	LOG( "ConfigureClocks: Oculus360Videos only needs minimal clocks" );
	modeParms.CpuLevel = 1;
	modeParms.GpuLevel = 0;

	// When the app is throttled, go to the platform UI and display a
	// dismissable warning. On return to the app, force 30hz timewarp.
	modeParms.AllowPowerSave = true;

	// All geometry is blended, so save power with no MSAA
	app->GetVrParms().multisamples = 1;
}

bool Oculus360Videos::OnKeyEvent( const int keyCode, const KeyState::eKeyEventType eventType )
{
	if ( ( ( keyCode == AKEYCODE_BACK ) && ( eventType == KeyState::KEY_EVENT_SHORT_PRESS ) ) ||
		( ( keyCode == KEYCODE_B ) && ( eventType == KeyState::KEY_EVENT_UP ) ) )
	{
		if ( MenuState == MENU_VIDEO_LOADING )
		{
			return true;
		}

		if ( ActiveVideo )
		{
			SetMenuState( MENU_BROWSER );
			return true;	// consume the key
		}
		// if no video is playing (either paused or stopped), let VrLib handle the back key
	}
	else if ( keyCode == AKEYCODE_P && eventType == KeyState::KEY_EVENT_DOWN )
	{
		PauseVideo( true );
	}

	return false;
}

void Oculus360Videos::Command( const char * msg )
{
	// Always include the space in MatchesHead to prevent problems
	// with commands with matching prefixes.

	if ( MatchesHead( "newVideo ", msg ) )
	{
		delete MovieTexture;
		MovieTexture = new SurfaceTexture( app->GetVrJni() );
		LOG( "RC_NEW_VIDEO texId %i", MovieTexture->textureId );

		MessageQueue	* receiver;
		sscanf( msg, "newVideo %p", &receiver );

		receiver->PostPrintf( "surfaceTexture %p", MovieTexture->javaObject );

		// don't draw the screen until we have the new size
		CurrentVideoWidth = 0;

		return;
	}
	else if ( MatchesHead( "completion", msg ) ) // video complete, return to menu
	{
		SetMenuState( MENU_BROWSER );
		return;
	}
	else if ( MatchesHead( "video ", msg ) )
	{
		sscanf( msg, "video %i %i", &CurrentVideoWidth, &CurrentVideoHeight );

		if ( MenuState != MENU_VIDEO_PLAYING ) // If video is already being played dont change the state to video ready
		{
			SetMenuState( MENU_VIDEO_READY );
		}

		return;
	}
	else if ( MatchesHead( "resume ", msg ) )
	{
		OnResume();
		return;	// allow VrLib to handle it, too
	}
	else if ( MatchesHead( "pause ", msg ) )
	{
		OnPause();
		return;	// allow VrLib to handle it, too
	}
	else if ( MatchesHead( "startError", msg ) )
	{
		// FIXME: this needs to do some parameter magic to fix xliff tags
		String message;
		VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/playback_failed", "@string/playback_failed", message );
		String fileName = ExtractFile( ActiveVideo->Url );
		message = VrLocale::GetXliffFormattedString( message, fileName );
		BitmapFont & font = app->GetDefaultFont();
		font.WordWrapText( message, 1.0f );
		app->ShowInfoText( 4.5f, message );
		SetMenuState( MENU_BROWSER );
		return;
	}

}

Matrix4f	Oculus360Videos::TexmForVideo( const int eye )
{
	if ( strstr( VideoName.ToCStr(), "_TB.mp4" ) )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0.5,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	}
	if ( strstr( VideoName.ToCStr(), "_BT.mp4" ) )
	{	// top / bottom stereo panorama
		return ( !eye ) ?
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0.5,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	}
	if ( strstr( VideoName.ToCStr(), "_LR.mp4" ) )
	{	// left / right stereo panorama
		return eye ?
			Matrix4f( 0.5, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f( 0.5, 0, 0, 0.5,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	}
	if ( strstr( VideoName.ToCStr(), "_RL.mp4" ) )
	{	// left / right stereo panorama
		return ( !eye ) ?
			Matrix4f( 0.5, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f( 0.5, 0, 0, 0.5,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );
	}

	// default to top / bottom stereo
	if ( CurrentVideoWidth == CurrentVideoHeight )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0.5,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f( 1, 0, 0, 0,
			0, 0.5, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );

		// We may want to support swapping top/bottom
	}
	return Matrix4f::Identity();
}

Matrix4f	Oculus360Videos::TexmForBackground( const int eye )
{
	if ( BackgroundWidth == BackgroundHeight )
	{	// top / bottom stereo panorama
		return eye ?
			Matrix4f(
			1, 0, 0, 0,
			0, 0.5, 0, 0.5,
			0, 0, 1, 0,
			0, 0, 0, 1 )
			:
			Matrix4f(
			1, 0, 0, 0,
			0, 0.5, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1 );

		// We may want to support swapping top/bottom
	}
	return Matrix4f::Identity();
}

Matrix4f Oculus360Videos::DrawEyeView( const int eye, const float fovDegrees )
{
	Matrix4f mvp = Scene.MvpForEye( eye, fovDegrees );

	if ( MenuState != MENU_VIDEO_PLAYING )
	{
		// Draw the ovr scene 
		const float fadeColor = CurrentFadeLevel;
		ModelDef & def = *const_cast< ModelDef * >( &Scene.WorldModel.Definition->Def );
		for ( int i = 0; i < def.surfaces.GetSizeI(); i++ )
		{
			SurfaceDef & sd = def.surfaces[ i ];
			glUseProgram( SingleColorTextureProgram.program );

			glUniformMatrix4fv( SingleColorTextureProgram.uMvp, 1, GL_FALSE, mvp.Transposed().M[ 0 ] );

			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, sd.materialDef.textures[ 0 ] );

			glUniform4f( SingleColorTextureProgram.uColor, fadeColor, fadeColor, fadeColor, 1.0f );

			sd.geo.Draw();

			glBindTexture( GL_TEXTURE_2D, 0 ); // don't leave it bound
		}
	}
	else if ( ( MenuState == MENU_VIDEO_PLAYING ) && ( MovieTexture != NULL ) )
	{
		// draw animated movie panorama
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, MovieTexture->textureId );

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, BackgroundTexId );

		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );

		GlProgram & prog = ( BackgroundWidth == BackgroundHeight ) ? FadedPanoramaProgram : PanoramaProgram;

		glUseProgram( prog.program );
		glUniform4f( prog.uColor, 1.0f, 1.0f, 1.0f, 1.0f );

		// Videos have center as initial focal point - need to rotate 90 degrees to start there
		const Matrix4f view = Scene.ViewMatrixForEye( 0 ) * Matrix4f::RotationY( M_PI / 2 );
		const Matrix4f proj = Scene.ProjectionMatrixForEye( 0, fovDegrees );

		const int toggleStereo = VideoMenu->IsOpenOrOpening() ? 0 : eye;

		glUniformMatrix4fv( prog.uTexm, 1, GL_FALSE, TexmForVideo( toggleStereo ).Transposed().M[ 0 ] );
		glUniformMatrix4fv( prog.uMvp, 1, GL_FALSE, ( proj * view ).Transposed().M[ 0 ] );
		Globe.Draw();

		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );	// don't leave it bound
	}

	return mvp;
}

float Fade( double now, double start, double length )
{
	return OVR::Alg::Clamp( ( ( now - start ) / length ), 0.0, 1.0 );
}

bool Oculus360Videos::IsVideoPlaying() const
{
	jmethodID methodId = app->GetVrJni()->GetMethodID( MainActivityClass, "isPlaying", "()Z" );
	if ( !methodId )
	{
		LOG( "Couldn't find isPlaying methodID" );
		return false;
	}

	bool isPlaying = app->GetVrJni()->CallBooleanMethod( app->GetJavaObject(), methodId );
	return isPlaying;
}

void Oculus360Videos::PauseVideo( bool const force )
{
	LOG( "PauseVideo()" );

	jmethodID methodId = app->GetVrJni()->GetMethodID( MainActivityClass,
		"pauseMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find pauseMovie methodID" );
		return;
	}

	app->GetVrJni()->CallVoidMethod( app->GetJavaObject(), methodId );
}

void Oculus360Videos::StopVideo()
{
	LOG( "StopVideo()" );

	jmethodID methodId = app->GetVrJni()->GetMethodID( MainActivityClass,
		"stopMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find stopMovie methodID" );
		return;
	}

	app->GetVrJni()->CallVoidMethod( app->GetJavaObject(), methodId );

	delete MovieTexture;
	MovieTexture = NULL;
}

void Oculus360Videos::ResumeVideo()
{
	LOG( "ResumeVideo()" );

	app->GetGuiSys().CloseMenu( app, Browser, false );

	jmethodID methodId = app->GetVrJni()->GetMethodID( MainActivityClass,
		"resumeMovie", "()V" );
	if ( !methodId )
	{
		LOG( "Couldn't find resumeMovie methodID" );
		return;
	}

	app->GetVrJni()->CallVoidMethod( app->GetJavaObject(), methodId );
}

void Oculus360Videos::StartVideo( const double nowTime )
{
	if ( ActiveVideo )
	{
		SetMenuState( MENU_VIDEO_LOADING );
		VideoName = ActiveVideo->Url;
		LOG( "StartVideo( %s )", ActiveVideo->Url.ToCStr() );
		app->PlaySound( "sv_select" );

		jmethodID startMovieMethodId = app->GetVrJni()->GetMethodID( MainActivityClass,
			"startMovieFromNative", "(Ljava/lang/String;)V" );

		if ( !startMovieMethodId )
		{
			LOG( "Couldn't find startMovie methodID" );
			return;
		}

		LOG( "moviePath = '%s'", ActiveVideo->Url.ToCStr() );
		jstring jstr = app->GetVrJni()->NewStringUTF( ActiveVideo->Url );
		app->GetVrJni()->CallVoidMethod( app->GetJavaObject(), startMovieMethodId, jstr );
		app->GetVrJni()->DeleteLocalRef( jstr );

		LOG( "StartVideo done" );
	}
}

void Oculus360Videos::SeekTo( const int seekPos )
{
	if ( ActiveVideo )
	{
		jmethodID seekToMethodId = app->GetVrJni()->GetMethodID( MainActivityClass,
			"seekToFromNative", "(I)V" );

		if ( !seekToMethodId )
		{
			LOG( "Couldn't find seekToMethodId methodID" );
			return;
		}

		app->GetVrJni()->CallVoidMethod( app->GetJavaObject(), seekToMethodId, seekPos );

		LOG( "SeekTo %i done", seekPos );
	}
}

void Oculus360Videos::SetMenuState( const OvrMenuState state )
{
	OvrMenuState lastState = MenuState;
	MenuState = state;
	LOG( "%s to %s", MenuStateString( lastState ), MenuStateString( MenuState ) );
	switch ( MenuState )
	{
	case MENU_NONE:
		break;
	case MENU_BROWSER:
		Fader.ForceFinish();
		Fader.Reset();
		app->GetGuiSys().CloseMenu( app, VideoMenu, false );
		app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrFolderBrowser::MENU_NAME );
		if ( ActiveVideo )
		{
			StopVideo();
			ActiveVideo = NULL;
		}
		break;
	case MENU_VIDEO_LOADING:
		if ( MovieTexture != NULL )
		{
			delete MovieTexture;
			MovieTexture = NULL;
		}
		app->GetGuiSys().CloseMenu( app, Browser, false );
		app->GetGuiSys().CloseMenu( app, VideoMenu, false );
		Fader.StartFadeOut();
		break;
	case MENU_VIDEO_READY:
		break;
	case MENU_VIDEO_PLAYING:
		Fader.Reset();
		VideoMenuTimeLeft = VideoMenuVisibleTime;
		break;
	default:
		LOG( "Oculus360Videos::SetMenuState unknown state: %d", static_cast< int >( state ) );
		OVR_ASSERT( false );
	}
}

const char * menuStateNames [ ] =
{
	"MENU_NONE",
	"MENU_BROWSER",
	"MENU_VIDEO_LOADING",
	"MENU_VIDEO_READY",
	"MENU_VIDEO_PLAYING",
	"NUM_MENU_STATES"
};

const char* Oculus360Videos::MenuStateString( const OvrMenuState state )
{
	OVR_ASSERT( state >= 0 && state < NUM_MENU_STATES );
	return menuStateNames[ state ];
}

void Oculus360Videos::OnVideoActivated( const OvrMetaDatum * videoData )
{
	ActiveVideo = videoData;
	StartVideo( ovr_GetTimeInSeconds() );
}

Matrix4f Oculus360Videos::Frame( const VrFrame vrFrame )
{
	// Disallow player foot movement, but we still want the head model
	// movement for the swipe view.
	VrFrame vrFrameWithoutMove = vrFrame;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 0 ] = 0.0f;
	vrFrameWithoutMove.Input.sticks[ 0 ][ 1 ] = 0.0f;
	Scene.Frame( app->GetVrViewParms(), vrFrameWithoutMove, app->GetSwapParms().ExternalVelocity );

	// Check for new video frames
	// latch the latest movie frame to the texture.
	if ( MovieTexture && CurrentVideoWidth ) {
		glActiveTexture( GL_TEXTURE0 );
		MovieTexture->Update();
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
		FrameAvailable = false;
	}

	if ( MenuState != MENU_BROWSER && MenuState != MENU_VIDEO_LOADING )
	{
		if ( vrFrame.Input.buttonReleased & ( BUTTON_TOUCH | BUTTON_A ) )
		{
			app->PlaySound( "sv_release_active" );
			if ( IsVideoPlaying() )
			{
				app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrVideoMenu::MENU_NAME );
				VideoMenu->RepositionMenu( app );
				PauseVideo( false );
			}
			else
			{
				app->GetGuiSys().CloseMenu( app, VideoMenu, false );
				ResumeVideo();
			}
		}
	}

	// State transitions
	if ( Fader.GetFadeState() != Fader::FADE_NONE )
	{
		Fader.Update( CurrentFadeRate, vrFrame.DeltaSeconds );
	}
	else if ( ( MenuState == MENU_VIDEO_READY ) &&
		( Fader.GetFadeAlpha() == 0.0f ) &&
		( MovieTexture != NULL ) )
	{
		SetMenuState( MENU_VIDEO_PLAYING );
		app->RecenterYaw( true );
	}
	CurrentFadeLevel = Fader.GetFinalAlpha();

	// We could disable the srgb convert on the FBO. but this is easier
	app->GetVrParms().colorFormat = UseSrgb ? COLOR_8888_sRGB : COLOR_8888;

	// Draw both eyes
	app->DrawEyeViewsPostDistorted( Scene.CenterViewMatrix() );

	return Scene.CenterViewMatrix();
}

void Oculus360Videos::OnResume()
{
	LOG( "Oculus360Videos::OnResume" );
	if ( VideoWasPlayingWhenPaused )
	{
		app->GetGuiSys().OpenMenu( app, app->GetGazeCursor(), OvrVideoMenu::MENU_NAME );
		VideoMenu->RepositionMenu( app );
		PauseVideo( false );
	}
}

void Oculus360Videos::OnPause()
{
	LOG( "Oculus360Videos::OnPause" );
	VideoWasPlayingWhenPaused = IsVideoPlaying();
	if ( VideoWasPlayingWhenPaused )
	{
		PauseVideo( false );
	}
}

}
