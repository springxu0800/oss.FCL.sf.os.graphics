// Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
//

/**
 @file
 @test
 @internalComponent
*/

#include <e32base.h>
#include <e32debug.h>
#include "t_simloadapp.h"
#include "t_simloadutils.h"
#include "t_wservconsts.h"

GLDEF_C TInt E32Main()
	{
	RDebug::Print(_L("T_SimLoadApp1::E32Main - entry"));

	CTrapCleanup* TheTrapCleanup = CTrapCleanup::New();

	TRAPD(err, StartTestL());
	if (err)
		{
		User::Panic(KTSimLoadAppPanic,err);
		}

    delete TheTrapCleanup;

	RDebug::Print(_L("T_SimLoadApp1::E32Main - exit"));
	return KErrNone;
	}

LOCAL_C void StartTestL(void)
	{
	TInt mySimLoad = CTSimLoadAppUtils::SimLoadL();
	
	// static simulated load
	if (CTSimLoadAppUtils::StaticLoadL())
		{
		// when simulating the static load there are 3 simload
		// apps executing at the same time. To harmonise the 
		// load a different number (seed value) is passed
		// to the engine, hence 100000 in this case
		CTSimLoadAppUtils::SimulateStaticLoadL(mySimLoad, 100000);
		}
	else 
		{
		// spiked simulated load
		if (CTSimLoadAppUtils::SpikedLoadL())
			{
			CTSimLoadAppUtils::SimulateSpikedLoad();
			}
		else
			{
			User::Leave(KErrAbort);
			}
		}
	}
