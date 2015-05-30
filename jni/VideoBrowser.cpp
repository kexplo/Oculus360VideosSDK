/************************************************************************************

Filename    :   VideoBrowser.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Videos/ directory. An additional grant 
of patent rights can be found in the PATENTS file in the same directory.

*************************************************************************************/

#include "VideoBrowser.h"
#include "GlTexture.h"
#include "Kernel/OVR_String_Utils.h"
#include "VrCommon.h"
#include "PackageFiles.h"
#include "ImageData.h"
#include "Oculus360Videos.h"
#include "VrLocale.h"
#include "BitmapFont.h"
#include <OVR_TurboJpeg.h>
#include "linux/stat.h"
#include <unistd.h>

namespace OVR
{

VideoBrowser * VideoBrowser::Create(
		App * app,
		OvrVideosMetaData & metaData,
		unsigned thumbWidth,
		float horizontalPadding,
		unsigned thumbHeight,
		float verticalPadding,
		unsigned 	numSwipePanels,
		float SwipeRadius )
{
	return new VideoBrowser( app, metaData,
		thumbWidth + horizontalPadding, thumbHeight + verticalPadding, SwipeRadius, numSwipePanels, thumbWidth, thumbHeight );
}

void VideoBrowser::OnPanelActivated( const OvrMetaDatum * panelData )
{
	Oculus360Videos * videos = ( Oculus360Videos * )AppPtr->GetAppInterface();
	OVR_ASSERT( videos );
	videos->OnVideoActivated( panelData );
}

unsigned char * VideoBrowser::CreateAndCacheThumbnail( const char * soureFile, const char * cacheDestinationFile, int & outW, int & outH )
{
	// TODO
	return NULL;
}

unsigned char * VideoBrowser::LoadThumbnail( const char * filename, int & width, int & height )
{
	LOG( "VideoBrowser::LoadThumbnail loading on %s", filename );
	unsigned char * orig = NULL;
		
	if ( strstr( filename, "assets/" ) )
	{
		void * buffer = NULL;
		int length = 0;
		ovr_ReadFileFromApplicationPackage( filename, length, buffer );

		if ( buffer )
		{
			orig = TurboJpegLoadFromMemory( reinterpret_cast< const unsigned char * >( buffer ), length, &width, &height );
			free( buffer );
		}
	}
	else if ( strstr( filename, ".pvr" ) )
	{	
		orig = LoadPVRBuffer( filename, width, height );
	}
	else
	{
		orig = TurboJpegLoadFromFile( filename, &width, &height );
	}

	if ( orig )
	{
		const int ThumbWidth = GetThumbWidth();
		const int ThumbHeight = GetThumbHeight();

		if ( ThumbWidth == width && ThumbHeight == height )
		{
			LOG( "VideoBrowser::LoadThumbnail skip resize on %s", filename );
			return orig;
		}

		LOG( "VideoBrowser::LoadThumbnail resizing %s to %ix%i", filename, ThumbWidth, ThumbHeight );
		unsigned char * outBuffer = ScaleImageRGBA( ( const unsigned char * )orig, width, height, ThumbWidth, ThumbHeight, IMAGE_FILTER_CUBIC );
		free( orig );
		
		if ( outBuffer )
		{
			width = ThumbWidth;
			height = ThumbHeight;

			return outBuffer;
		}
	}
	else
	{
		LOG( "Error: VideoBrowser::LoadThumbnail failed to load %s", filename );
	}
	return NULL;
}

String VideoBrowser::ThumbName( const String & s )
{
	String	ts( s );
	ts = OVR::StringUtils::SetFileExtensionString( ts, ".pvr" );
	return ts;
}

String VideoBrowser::AlternateThumbName( const String & s )
{
	String	ts( s );
	ts = OVR::StringUtils::SetFileExtensionString( ts, ".thm" );
	return ts;
}


void VideoBrowser::OnMediaNotFound( App * app, String & title, String & imageFile, String & message )
{
	VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/app_name", "@string/app_name", title );
	imageFile = "assets/sdcard.png";
	VrLocale::GetString( app->GetVrJni(), app->GetJavaObject(), "@string/media_not_found", "@string/media_not_found", message );
	BitmapFont & font = app->GetDefaultFont();
	OVR::Array< OVR::String > wholeStrs;
	wholeStrs.PushBack( "Gear VR" );
	font.WordWrapText( message, 1.4f, wholeStrs );
}

String VideoBrowser::GetCategoryTitle( char const * key, char const * defaultStr ) const
{
	String outStr;
	VrLocale::GetString( AppPtr->GetVrJni(), AppPtr->GetJavaObject(), key, defaultStr, outStr );
	return outStr;
}

String VideoBrowser::GetPanelTitle( const OvrMetaDatum & panelData ) const
{
	const OvrVideosMetaDatum * const videosDatum = static_cast< const OvrVideosMetaDatum * const >( &panelData );
	if ( videosDatum != NULL )
	{
		return videosDatum->Title;
	}
	return String();
}

}
