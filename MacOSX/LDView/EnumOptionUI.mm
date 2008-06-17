//
//  EnumOptionUI.mm
//  LDView
//
//  Created by Travis Cobbs on 6/16/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "EnumOptionUI.h"
#import "LDViewCategories.h"

#include <LDExporter/LDExporterSetting.h>


@implementation EnumOptionUI

- (id)initWithOptions:(Options *)theOptions setting:(LDExporterSetting &)theSetting
{
	self = [super initWithOptions:theOptions setting:theSetting];
	if (self != nil)
	{
		const UCStringVector &values = setting->getOptions();
		size_t i;

		label = [self createLabel];
		popUpButton = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
		for (i = 0; i < values.size(); i++)
		{
			[popUpButton addItemWithTitle:[NSString stringWithUCString:values[i]]];
		}
	}
	return self;
}

- (void)dealloc
{
	if (!shown)
	{
		[label release];
		[popUpButton release];
	}
	[super dealloc];
}

- (float)updateLayoutX:(float)x y:(float)y width:(float)width update:(bool)update optimalWidth:(float &)optimalWidth
{
	NSRect labelFrame = { { x, y }, { width, 1024.0f } };
	NSRect popUpFrame;

	labelFrame = [self calcLabelLayout:label inRect:labelFrame optimalWidth:optimalWidth];
	[popUpButton sizeToFit];
	popUpFrame = [popUpButton frame];
	popUpFrame.origin.x = x;
	popUpFrame.origin.y = y + labelFrame.size.height;
	popUpFrame.size.width = width;
	if (update)
	{
		[label setFrame:labelFrame];
		[popUpButton setFrame:popUpFrame];
		if (!shown)
		{
			shown = true;
			[docView addSubview:label];
			[label release];
			[docView addSubview:popUpButton];
			[popUpButton release];
		}
	}
	return popUpFrame.origin.y + popUpFrame.size.height - labelFrame.origin.y;
}

- (void)commit
{
}

- (void)setEnabled:(BOOL)enabled
{
}

- (NSRect)frame
{
	return NSMakeRect(0, 0, 0, 0);
}

@end