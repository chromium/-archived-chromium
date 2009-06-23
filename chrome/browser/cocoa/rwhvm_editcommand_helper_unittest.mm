// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/rwhvm_editcommand_helper.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class RWHVMEditCommandHelperTest : public PlatformTest {
};

// Bare bones obj-c class for testing purposes.
@interface RWHVMEditCommandHelperTestClass : NSObject
@end

@implementation RWHVMEditCommandHelperTestClass
@end

// Class that owns a RenderWidgetHostViewMac.
@interface RenderWidgetHostViewMacOwner :
    NSObject<RenderWidgetHostViewMacOwner> {
  RenderWidgetHostViewMac* rwhvm_;
}

- (id) initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm;
@end

@implementation RenderWidgetHostViewMacOwner

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm {
  if ((self = [super init])) {
    rwhvm_ = rwhvm;
  }
  return self;
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return rwhvm_;
}

@end


namespace {
  // Returns true if all the edit command names in the array are present
  // in test_obj.
  // edit_commands is a list of NSStrings, selector names are formed by
  // appending a trailing ':' to the string.
  bool CheckObjectRespondsToEditCommands(NSArray* edit_commands, id test_obj) {
    for (NSString* edit_command_name in edit_commands) {
      NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
      if (![test_obj respondsToSelector:NSSelectorFromString(sel_str)]) {
        return false;
      }
    }

    return true;
  }
}  // namespace

// Create a Mock RenderWidget
class MockRenderWidgetHostEditCommandCounter : public RenderWidgetHost {
 public:
  MockRenderWidgetHostEditCommandCounter(RenderProcessHost* process,
                                         int routing_id) :
    RenderWidgetHost(process, routing_id) {}

  MOCK_METHOD2(ForwardEditCommand, void(const std::string&,
      const std::string&));
};


// Tests that editing commands make it through the pipeline all the way to
// RenderWidgetHost.
TEST_F(RWHVMEditCommandHelperTest, TestEditingCommandDelivery) {
  RWHVMEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();

  // Set up a mock render widget and set expectations.
  MessageLoopForUI message_loop;
  TestingProfile profile;
  MockRenderProcessHost mock_process(&profile);
  MockRenderWidgetHostEditCommandCounter mock_render_widget(&mock_process, 0);

  size_t num_edit_commands = [edit_command_strings count];
  EXPECT_CALL(mock_render_widget,
        ForwardEditCommand(testing::_, testing::_)).Times(num_edit_commands);

// TODO(jeremy): Figure this out and reenable this test.
// For some bizarre reason this code doesn't work, running the code in the
// debugger confirms that the function is called with the correct parameters
// however gmock appears not to be able to match up parameters correctly.
// Disable for now till we can figure this out.
#if 0
  // Tell Mock object that we expect to recieve each edit command once.
  std::string empty_str;
  for (NSString* edit_command_name in edit_command_strings) {
    std::string command([edit_command_name UTF8String]);
    EXPECT_CALL(mock_render_widget,
        ForwardEditCommand(command, empty_str)).Times(1);
  }
#endif  // 0

  // RenderWidgetHostViewMac self destructs (RenderWidgetHostViewMacCocoa
  // takes ownership) so no need to delete it ourselves.
  RenderWidgetHostViewMac* rwhvm = new RenderWidgetHostViewMac(
      &mock_render_widget);

  RenderWidgetHostViewMacOwner* rwhwvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc]
          initWithRenderWidgetHostViewMac:rwhvm] autorelease];

  helper.AddEditingSelectorsToClass([rwhwvm_owner class]);

  for (NSString* edit_command_name in edit_command_strings) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    [rwhwvm_owner performSelector:NSSelectorFromString(sel_str) withObject:nil];
  }
}

// Test RWHVMEditCommandHelper::AddEditingSelectorsToClass
TEST_F(RWHVMEditCommandHelperTest, TestAddEditingSelectorsToClass) {
  RWHVMEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();
  ASSERT_GT([edit_command_strings count], 0U);

  // Create a class instance and add methods to the class.
  RWHVMEditCommandHelperTestClass* test_obj =
      [[[RWHVMEditCommandHelperTestClass alloc] init] autorelease];

  // Check that edit commands aren't already attached to the object.
  ASSERT_FALSE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands where added.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  // AddEditingSelectorsToClass() should be idempotent.
  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands are still there.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));
}

// Test RWHVMEditCommandHelper::IsMenuItemEnabled.
TEST_F(RWHVMEditCommandHelperTest, TestMenuItemEnabling) {
  RWHVMEditCommandHelper helper;
  RenderWidgetHostViewMacOwner* rwhvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc] init] autorelease];

  // The select all menu should always be enabled.
  SEL select_all = NSSelectorFromString(@"selectAll:");
  ASSERT_TRUE(helper.IsMenuItemEnabled(select_all, rwhvm_owner));

  // Random selectors should be enabled by the function.
  SEL garbage_selector = NSSelectorFromString(@"randomGarbageSelector:");
  ASSERT_FALSE(helper.IsMenuItemEnabled(garbage_selector, rwhvm_owner));

  // TODO(jeremy): Currently IsMenuItemEnabled just returns true for all edit
  // selectors.  Once we go past that we should do more extensive testing here.
}
