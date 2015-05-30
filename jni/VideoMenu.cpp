/************************************************************************************

Filename    :   VideoMenu.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VideoMenu.h"

#include "VRMenu/VRMenuMgr.h"
#include "VRMenu/GuiSys.h"
#include "VRMenu/DefaultComponent.h"
#include "VRMenu/ActionComponents.h"
#include "VRMenu/FolderBrowser.h"

#include "Oculus360Videos.h"
#include "VideosMetaData.h"

namespace OVR {

const VRMenuId_t OvrVideoMenu::ID_CENTER_ROOT( 1000 );
const VRMenuId_t OvrVideoMenu::ID_BROWSER_BUTTON( 1000 + 1011 );
const VRMenuId_t OvrVideoMenu::ID_VIDEO_BUTTON( 1000 + 1012 );

char const * OvrVideoMenu::MENU_NAME = "VideoMenu";

static const Vector3f FWD( 0.0f, 0.0f, 1.0f );
static const Vector3f RIGHT( 1.0f, 0.0f, 0.0f );
static const Vector3f UP( 0.0f, 1.0f, 0.0f );
static const Vector3f DOWN( 0.0f, -1.0f, 0.0f );

static const int BUTTON_COOL_DOWN_SECONDS = 0.25f;

char *itoa(int num, char *str, int radix) {
	char sign = 0;
	char temp[17];  //an int can only be 16 bits long
	//at radix 2 (binary) the string
	//is at most 16 + 1 null long.
	int temp_loc = 0;
	int digit;
	int str_loc = 0;

	//save sign for radix 10 conversion
	if (radix == 10 && num < 0) {
		sign = 1;
		num = -num;
	}

	//construct a backward string of the number.
	do {
		digit = (unsigned int)num % radix;
		if (digit < 10)
			temp[temp_loc++] = digit + '0';
		else
			temp[temp_loc++] = digit - 10 + 'A';
		num = (((unsigned int)num) / radix);
	} while ((unsigned int)num > 0);

	//now add the sign for radix 10
	if (radix == 10 && sign) {
		temp[temp_loc] = '-';
	}
	else {
		temp_loc--;
	}


	//now reverse the string.
	while (temp_loc >= 0) {// while there are still chars
		str[str_loc++] = temp[temp_loc--];
	}
	str[str_loc] = 0; // add null termination.

	return str;
}

//==============================
// OvrVideoMenuRootComponent
// This component is attached to the root of VideoMenu 
class OvrVideoMenuRootComponent : public VRMenuComponent
{
public:
	OvrVideoMenuRootComponent( OvrVideoMenu & videoMenu )
		: VRMenuComponent( VRMenuEventFlags_t( VRMENU_EVENT_FRAME_UPDATE ) | VRMENU_EVENT_OPENING )
		, VideoMenu( videoMenu )
		, CurrentVideo( NULL )
	{
	}

private:
	virtual eMsgStatus	OnEvent_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr,
		VRMenuObject * self, VRMenuEvent const & event )
	{
		switch ( event.EventType )
		{
		case VRMENU_EVENT_FRAME_UPDATE:
			return OnFrame( app, vrFrame, menuMgr, self, event );
		case VRMENU_EVENT_OPENING:
			return OnOpening( app, vrFrame, menuMgr, self, event );
		default:
			OVR_ASSERT( !"Event flags mismatch!" ); // the constructor is specifying a flag that's not handled
			return MSG_STATUS_ALIVE;
		}
	}

	eMsgStatus OnOpening( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		CurrentVideo = (OvrVideosMetaDatum *)( VideoMenu.GetVideos()->GetActiveVideo() );
		// If opening VideoMenu without a Video selected, bail 
		if ( CurrentVideo == NULL )
		{
			app->GetGuiSys().CloseMenu( app, &VideoMenu, false );
		}
		LoadAttribution( self );
		return MSG_STATUS_CONSUMED;
	}

	void LoadAttribution( VRMenuObject * self )
	{
		if ( CurrentVideo )
		{
			int duration = VideoMenu.GetVideos()->GetDuration();
			int curPos = VideoMenu.GetVideos()->GetCurrentPosition();

			char buf[256] = { 0 };
			itoa(curPos, buf, 10);

			String titleWithPos = CurrentVideo->Title + "\n(" + buf + "/";

			memset(buf, 0, 256);
			itoa(duration, buf, 10);

			titleWithPos += buf;
			titleWithPos += ")";
			self->SetText( titleWithPos );
		}
	}

	eMsgStatus OnFrame( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, VRMenuObject * self, VRMenuEvent const & event )
	{
		return MSG_STATUS_ALIVE;
	}

private:
	OvrVideoMenu &			VideoMenu;
	OvrVideosMetaDatum *	CurrentVideo;
};

//==============================
// OvrVideoMenu
OvrVideoMenu * OvrVideoMenu::Create( App * app, Oculus360Videos * videos, OvrVRMenuMgr & menuMgr, BitmapFont const & font, OvrMetaData & metaData, float fadeOutTime, float radius )
{
	return new OvrVideoMenu( app, videos, menuMgr, font, metaData, fadeOutTime, radius );
}

OvrVideoMenu::OvrVideoMenu( App * app, Oculus360Videos * videos, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
	OvrMetaData & metaData, float fadeOutTime, float radius )
	: VRMenu( MENU_NAME )
	, AppPtr( app )
	, MenuMgr( menuMgr )
	, Font( font )
	, Videos( videos )
	, MetaData( metaData )
	, LoadingIconHandle( 0 )
	, AttributionHandle( 0 )
	, BrowserButtonHandle( 0 )
	, VideoControlButtonHandle( 0 )
	, Radius( radius )
	, ButtonCoolDown( 0.0f )
	, OpenTime( 0.0 )
{
	// Init with empty root
	Init( menuMgr, font, 0.0f, VRMenuFlags_t() );

	// Create Attribution info view
	Array< VRMenuObjectParms const * > parms;
	Array< VRMenuComponent* > comps;
	VRMenuId_t attributionPanelId( ID_CENTER_ROOT.Get() + 10 );

	comps.PushBack( new OvrVideoMenuRootComponent( *this ) );

	Quatf rot( DOWN, 0.0f );
	Vector3f dir( -FWD );
	Posef panelPose( rot, dir * Radius );
	Vector3f panelScale( 1.0f );

	const VRMenuFontParms fontParms( true, true, false, false, true, 0.525f, 0.45f, 1.0f );

	VRMenuObjectParms attrParms( VRMENU_STATIC, comps,
		VRMenuSurfaceParms(), "Attribution Panel", panelPose, panelScale, Posef(), Vector3f( 1.0f ), fontParms, attributionPanelId,
		VRMenuObjectFlags_t(), VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );

	parms.PushBack( &attrParms );

	AddItems( MenuMgr, Font, parms, GetRootHandle(), false );
	parms.Clear();
	comps.Clear();

	AttributionHandle = HandleForId( MenuMgr, attributionPanelId );
	VRMenuObject * attributionObject = MenuMgr.ToObject( AttributionHandle );
	OVR_ASSERT( attributionObject != NULL );

	//Browser button
	float const ICON_HEIGHT = 80.0f * VRMenuObject::DEFAULT_TEXEL_SCALE;
	Array< VRMenuSurfaceParms > surfParms;

	Posef browserButtonPose( Quatf(), UP * ICON_HEIGHT * 2.0f );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_BROWSER_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent( ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"assets/nav_home_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"assets/nav_home_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	VRMenuObjectParms browserButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		browserButtonPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms,
		ID_BROWSER_BUTTON, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &browserButtonParms );

	AddItems( MenuMgr, Font, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();
	surfParms.Clear();

	BrowserButtonHandle = attributionObject->ChildHandleForId( MenuMgr, ID_BROWSER_BUTTON );
	VRMenuObject * browserButtonObject = MenuMgr.ToObject( BrowserButtonHandle );
	OVR_ASSERT( browserButtonObject != NULL );
	OVR_UNUSED( browserButtonObject );

	//Video control button 
	Posef videoButtonPose( Quatf(), DOWN * ICON_HEIGHT * 2.0f );

	comps.PushBack( new OvrDefaultComponent( Vector3f( 0.0f, 0.0f, 0.05f ), 1.05f, 0.25f, 0.0f, Vector4f( 1.0f ), Vector4f( 1.0f ) ) );
	comps.PushBack( new OvrButton_OnUp( this, ID_VIDEO_BUTTON ) );
	comps.PushBack( new OvrSurfaceToggleComponent( ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"assets/nav_restart_off.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	surfParms.PushBack( VRMenuSurfaceParms( "browser",
		"assets/nav_restart_on.png", SURFACE_TEXTURE_DIFFUSE,
		NULL, SURFACE_TEXTURE_MAX, NULL, SURFACE_TEXTURE_MAX ) );
	VRMenuObjectParms controlButtonParms( VRMENU_BUTTON, comps, surfParms, "",
		videoButtonPose, Vector3f( 1.0f ), Posef(), Vector3f( 1.0f ), fontParms,
		ID_VIDEO_BUTTON, VRMenuObjectFlags_t( VRMENUOBJECT_DONT_HIT_TEXT ),
		VRMenuObjectInitFlags_t( VRMENUOBJECT_INIT_FORCE_POSITION ) );
	parms.PushBack( &controlButtonParms );

	AddItems( MenuMgr, Font, parms, AttributionHandle, false );
	parms.Clear();
	comps.Clear();

	VideoControlButtonHandle = attributionObject->ChildHandleForId( MenuMgr, ID_VIDEO_BUTTON );
	VRMenuObject * controlButtonObject = MenuMgr.ToObject( VideoControlButtonHandle );
	OVR_ASSERT( controlButtonObject != NULL );
	OVR_UNUSED( controlButtonObject );

}

OvrVideoMenu::~OvrVideoMenu()
{

}

void OvrVideoMenu::Open_Impl( App * app, OvrGazeCursor & gazeCursor )
{
	ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

	OpenTime = ovr_GetTimeInSeconds();
}

void OvrVideoMenu::Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font, BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId )
{
	if ( ButtonCoolDown > 0.0f )
	{
		ButtonCoolDown -= vrFrame.DeltaSeconds;
	}
}

void OvrVideoMenu::OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event )
{
	const double now = ovr_GetTimeInSeconds();
	if ( ButtonCoolDown <= 0.0f && (now - OpenTime > 0.5))
	{
		ButtonCoolDown = BUTTON_COOL_DOWN_SECONDS;

		if ( itemId.Get() == ID_BROWSER_BUTTON.Get() )
		{
			Videos->SetMenuState( Oculus360Videos::MENU_BROWSER );
		}
		else if ( itemId.Get( ) == ID_VIDEO_BUTTON.Get( ) )
		{
			Videos->SeekTo( 0 );
		}
	}
}

}
