/************************************************************************************

Filename    :	VideoMenu.h
Content     :   VRMenu shown when within panorama
Created     :   October 9, 2014
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#if !defined( OVR_VideoMenu_h )
#define OVR_VideoMenu_h

#include "VRMenu/VRMenu.h"

namespace OVR {

class Oculus360Videos;
class SearchPaths;
class OvrVideoMenuRootComponent;

class OvrMetaData;
struct OvrMetaDatum;

//==============================================================
// OvrPanoMenu
class OvrVideoMenu : public VRMenu
{
public:
	static char const *	MENU_NAME;
	static const VRMenuId_t ID_CENTER_ROOT;
	static const VRMenuId_t	ID_BROWSER_BUTTON;
	static const VRMenuId_t	ID_VIDEO_BUTTON;

	// only one of these every needs to be created
	static  OvrVideoMenu *		Create(
		App * app,
		Oculus360Videos * videos,
		OvrVRMenuMgr & menuMgr,
		BitmapFont const & font,
		OvrMetaData & metaData,
		float fadeOutTime,
		float radius );

	Oculus360Videos *		GetVideos( ) 					{ return Videos; }
	OvrMetaData & 			GetMetaData() 					{ return MetaData; }

private:
	OvrVideoMenu( App * app, Oculus360Videos * videos, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
		OvrMetaData & metaData, float fadeOutTime, float radius );

	virtual ~OvrVideoMenu( );
	virtual void			Open_Impl( App * app, OvrGazeCursor & gazeCursor );
	virtual void			OnItemEvent_Impl( App * app, VRMenuId_t const itemId, VRMenuEvent const & event );
	virtual void			Frame_Impl( App * app, VrFrame const & vrFrame, OvrVRMenuMgr & menuMgr, BitmapFont const & font,
		BitmapFontSurface & fontSurface, gazeCursorUserId_t const gazeUserId );

	// Globals
	App *					AppPtr;
	OvrVRMenuMgr &			MenuMgr;
	const BitmapFont &		Font;
	Oculus360Videos *		Videos;
	OvrMetaData & 			MetaData;

	menuHandle_t			LoadingIconHandle;
	menuHandle_t			AttributionHandle;
	menuHandle_t			BrowserButtonHandle;
	menuHandle_t			VideoControlButtonHandle;

	const float				Radius;

	float					ButtonCoolDown;

	double					OpenTime;

};

}

#endif
