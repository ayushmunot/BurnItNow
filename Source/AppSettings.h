/*
 * Copyright 2016. All rights reserved.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *	Humdinger, humdingerb@gmail.com
 */

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <Locker.h>
#include <Path.h>
#include <Rect.h>
#include <String.h>

class AppSettings {
public:
					AppSettings();
					~AppSettings();

		bool		Lock();
		void		Unlock();

		void		GetCacheFolder(BPath& folder);
		bool		GetEject();
		bool		GetCache();
		int32		GetSpeed();
		BRect		GetWindowPosition();
		void		GetSplitWeight(float& left, float& right);
		void		GetSplitCollapse(bool& left, bool& right);

		void		SetCacheFolder(BString folder);
		void		SetEject(bool eject);
		void		SetCache(bool cache);
		void		SetSpeed(int32 speed);
		void		SetWindowPosition(BRect where);
		void		SetSplitWeight(float left, float right);
		void		SetSplitCollapse(bool left, bool right);
private:
		void		_EnsureCacheIsValid();

		BString		fFolder;
		bool		fEject;
		bool		fCache;
		int32		fSpeed;
		BRect		fPosition;
		float		fInfoWeight;
		float		fTracksWeight;
		bool		fInfoCollapse;
		bool		fTracksCollapse;

		bool		dirtySettings;
		
		BLocker		fLock;
};

#endif	/* APPSETTINGS_H */
