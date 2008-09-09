// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/message_pump_win.h"
#include "base/scoped_handle.h"
#endif

using base::Thread;

// TODO(darin): Platform-specific MessageLoop tests should be grouped together
// to avoid chopping this file up with so many #ifdefs.

namespace {

class MessageLoopTest : public testing::Test {};

class Foo : public base::RefCounted<Foo> {
 public:
  Foo() : test_count_(0) {
  }

  void Test0() {
    ++test_count_;
  }

  void Test1ConstRef(const std::string& a) {
    ++test_count_;
    result_.append(a);
  }

  void Test1Ptr(std::string* a) {
    ++test_count_;
    result_.append(*a);
  }

  void Test1Int(int a) {
    test_count_ += a;
  }

  void Test2Ptr(std::string* a, std::string* b) {
    ++test_count_;
    result_.append(*a);
    result_.append(*b);
  }

  void Test2Mixed(const std::string& a, std::string* b) {
    ++test_count_;
    result_.append(a);
    result_.append(*b);
  }

  int test_count() const { return test_count_; }
  const std::string& result() const { return result_; }

 private:
  int test_count_;
  std::string result_;
};

class QuitMsgLoop : public base::RefCounted<QuitMsgLoop> {
 public:
  void QuitNow() {
    MessageLoop::current()->Quit();
  }
};

void RunTest_PostTask(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Add tests to message loop
  scoped_refptr<Foo> foo = new Foo();
  std::string a("a"), b("b"), c("c"), d("d");
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test0));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
    foo.get(), &Foo::Test1ConstRef, a));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test1Ptr, &b));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test1Int, 100));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test2Ptr, &a, &c));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
    foo.get(), &Foo::Test2Mixed, a, &d));

  // After all tests, post a message that will shut down the message loop
  scoped_refptr<QuitMsgLoop> quit = new QuitMsgLoop();
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      quit.get(), &QuitMsgLoop::QuitNow));

  // Now kick things off
  MessageLoop::current()->Run();

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

void RunTest_PostTask_SEH(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Add tests to message loop
  scoped_refptr<Foo> foo = new Foo();
  std::string a("a"), b("b"), c("c"), d("d");
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test0));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test1ConstRef, a));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test1Ptr, &b));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test1Int, 100));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test2Ptr, &a, &c));
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      foo.get(), &Foo::Test2Mixed, a, &d));

  // After all tests, post a message that will shut down the message loop
  scoped_refptr<QuitMsgLoop> quit = new QuitMsgLoop();
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      quit.get(), &QuitMsgLoop::QuitNow));

  // Now kick things off with the SEH block active.
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

// This class runs slowly to simulate a large amount of work being done.
class SlowTask : public Task {
 public:
  SlowTask(int pause_ms, int* quit_counter)
      : pause_ms_(pause_ms), quit_counter_(quit_counter) {
  }
  virtual void Run() {
    PlatformThread::Sleep(pause_ms_);
    if (--(*quit_counter_) == 0)
      MessageLoop::current()->Quit();
  }
 private:
  int pause_ms_;
  int* quit_counter_;
};

// This class records the time when Run was called in a Time object, which is
// useful for building a variety of MessageLoop tests.
class RecordRunTimeTask : public SlowTask {
 public:
  RecordRunTimeTask(Time* run_time, int* quit_counter)
      : SlowTask(10, quit_counter), run_time_(run_time) {
  }
  virtual void Run() {
    *run_time_ = Time::Now();
    // Cause our Run function to take some time to execute.  As a result we can
    // count on subsequent RecordRunTimeTask objects running at a future time,
    // without worry about the resolution of our system clock being an issue.
    SlowTask::Run();
  }
 private:
  Time* run_time_;
};

void RunTest_PostDelayedTask_Basic(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that PostDelayedTask results in a delayed task.

  const int kDelayMS = 100;

  int num_tasks = 1;
  Time run_time;

  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time, &num_tasks), kDelayMS);

  Time time_before_run = Time::Now();
  loop.Run();
  Time time_after_run = Time::Now();

  EXPECT_EQ(0, num_tasks);
  EXPECT_LT(kDelayMS, (time_after_run - time_before_run).InMilliseconds());
}

void RunTest_PostDelayedTask_InDelayOrder(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that two tasks with different delays run in the right order.

  int num_tasks = 2;
  Time run_time1, run_time2;

  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time1, &num_tasks), 200);
  // If we get a large pause in execution (due to a context switch) here, this
  // test could fail.
  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time2, &num_tasks), 10);

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 < run_time1);
}

void RunTest_PostDelayedTask_InPostOrder(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that two tasks with the same delay run in the order in which they
  // were posted.
  //
  // NOTE: This is actually an approximate test since the API only takes a
  // "delay" parameter, so we are not exactly simulating two tasks that get
  // posted at the exact same time.  It would be nice if the API allowed us to
  // specify the desired run time.

  const int kDelayMS = 100;

  int num_tasks = 2;
  Time run_time1, run_time2;

  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time1, &num_tasks), kDelayMS);
  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time2, &num_tasks), kDelayMS);

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time1 < run_time2);
}

void RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that a delayed task still runs after a normal tasks even if the
  // normal tasks take a long time to run.

  const int kPauseMS = 50;

  int num_tasks = 2;
  Time run_time;

  loop.PostTask(
      FROM_HERE, new SlowTask(kPauseMS, &num_tasks));
  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time, &num_tasks), 10);

  Time time_before_run = Time::Now();
  loop.Run();
  Time time_after_run = Time::Now();

  EXPECT_EQ(0, num_tasks);

  EXPECT_LT(kPauseMS, (time_after_run - time_before_run).InMilliseconds());
}

void RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // Test that a delayed task still runs after a pile of normal tasks.  The key
  // difference between this test and the previous one is that here we return
  // the MessageLoop a lot so we give the MessageLoop plenty of opportunities
  // to maybe run the delayed task.  It should know not to do so until the
  // delayed task's delay has passed.

  int num_tasks = 11;
  Time run_time1, run_time2;

  // Clutter the ML with tasks.
  for (int i = 1; i < num_tasks; ++i)
    loop.PostTask(FROM_HERE, new RecordRunTimeTask(&run_time1, &num_tasks));

  loop.PostDelayedTask(
      FROM_HERE, new RecordRunTimeTask(&run_time2, &num_tasks), 1);

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 > run_time1);
}

class NestingTest : public Task {
 public:
  explicit NestingTest(int* depth) : depth_(depth) {
  }
  void Run() {
    if (*depth_ > 0) {
      *depth_ -= 1;
      MessageLoop::current()->PostTask(FROM_HERE, new NestingTest(depth_));

      MessageLoop::current()->SetNestableTasksAllowed(true);
      MessageLoop::current()->Run();
    }
    MessageLoop::current()->Quit();
  }
 private:
  int* depth_;
};

#if defined(OS_WIN)

LONG WINAPI BadExceptionHandler(EXCEPTION_POINTERS *ex_info) {
  ADD_FAILURE() << "bad exception handler";
  ::ExitProcess(ex_info->ExceptionRecord->ExceptionCode);
  return EXCEPTION_EXECUTE_HANDLER;
}

// This task throws an SEH exception: initially write to an invalid address.
// If the right SEH filter is installed, it will fix the error.
class CrasherTask : public Task {
 public:
  // Ctor. If trash_SEH_handler is true, the task will override the unhandled
  // exception handler with one sure to crash this test.
  explicit CrasherTask(bool trash_SEH_handler)
      : trash_SEH_handler_(trash_SEH_handler) {
  }
  void Run() {
    PlatformThread::Sleep(1);
    if (trash_SEH_handler_)
      ::SetUnhandledExceptionFilter(&BadExceptionHandler);
    // Generate a SEH fault. We do it in asm to make sure we know how to undo
    // the damage.

#if defined(_M_IX86)

    __asm {
      mov eax, dword ptr [CrasherTask::bad_array_]
      mov byte ptr [eax], 66
    }

#elif defined(_M_X64)

    bad_array_[0] = 66;

#else
#error "needs architecture support"
#endif

    MessageLoop::current()->Quit();
  }
  // Points the bad array to a valid memory location.
  static void FixError() {
    bad_array_ = &valid_store_;
  }

 private:
  bool trash_SEH_handler_;
  static volatile char* bad_array_;
  static char valid_store_;
};

volatile char* CrasherTask::bad_array_ = 0;
char CrasherTask::valid_store_ = 0;

// This SEH filter fixes the problem and retries execution. Fixing requires
// that the last instruction: mov eax, [CrasherTask::bad_array_] to be retried
// so we move the instruction pointer 5 bytes back.
LONG WINAPI HandleCrasherTaskException(EXCEPTION_POINTERS *ex_info) {
  if (ex_info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
    return EXCEPTION_EXECUTE_HANDLER;

  CrasherTask::FixError();

#if defined(_M_IX86)

  ex_info->ContextRecord->Eip -= 5;

#elif defined(_M_X64)

  ex_info->ContextRecord->Rip -= 5;

#endif

  return EXCEPTION_CONTINUE_EXECUTION;
}

void RunTest_Crasher(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  if (::IsDebuggerPresent())
    return;

  LPTOP_LEVEL_EXCEPTION_FILTER old_SEH_filter =
      ::SetUnhandledExceptionFilter(&HandleCrasherTaskException);

  MessageLoop::current()->PostTask(FROM_HERE, new CrasherTask(false));
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  ::SetUnhandledExceptionFilter(old_SEH_filter);
}

void RunTest_CrasherNasty(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);
  
  if (::IsDebuggerPresent())
    return;

  LPTOP_LEVEL_EXCEPTION_FILTER old_SEH_filter =
      ::SetUnhandledExceptionFilter(&HandleCrasherTaskException);

  MessageLoop::current()->PostTask(FROM_HERE, new CrasherTask(true));
  MessageLoop::current()->set_exception_restoration(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->set_exception_restoration(false);

  ::SetUnhandledExceptionFilter(old_SEH_filter);
}

#endif  // defined(OS_WIN)

void RunTest_Nesting(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  int depth = 100;
  MessageLoop::current()->PostTask(FROM_HERE, new NestingTest(&depth));
  MessageLoop::current()->Run();
  EXPECT_EQ(depth, 0);
}

const wchar_t* const kMessageBoxTitle = L"MessageLoop Unit Test";

enum TaskType {
  MESSAGEBOX,
  ENDDIALOG,
  RECURSIVE,
  TIMEDMESSAGELOOP,
  QUITMESSAGELOOP,
  ORDERERD,
  PUMPS,
};

// Saves the order in which the tasks executed.
struct TaskItem {
  TaskItem(TaskType t, int c, bool s)
      : type(t),
        cookie(c),
        start(s) {
  }

  TaskType type;
  int cookie;
  bool start;

  bool operator == (const TaskItem& other) const {
    return type == other.type && cookie == other.cookie && start == other.start;
  }
};

typedef std::vector<TaskItem> TaskList;

std::ostream& operator <<(std::ostream& os, TaskType type) {
  switch (type) {
  case MESSAGEBOX:        os << "MESSAGEBOX"; break;
  case ENDDIALOG:         os << "ENDDIALOG"; break;
  case RECURSIVE:         os << "RECURSIVE"; break;
  case TIMEDMESSAGELOOP:  os << "TIMEDMESSAGELOOP"; break;
  case QUITMESSAGELOOP:   os << "QUITMESSAGELOOP"; break;
  case ORDERERD:          os << "ORDERERD"; break;
  case PUMPS:             os << "PUMPS"; break;
  default:
    NOTREACHED();
    os << "Unknown TaskType";
    break;
  }
  return os;
}

std::ostream& operator <<(std::ostream& os, const TaskItem& item) {
  if (item.start)
    return os << item.type << " " << item.cookie << " starts";
  else
    return os << item.type << " " << item.cookie << " ends";
}

// Saves the order the tasks ran.
class OrderedTasks : public Task {
 public:
  OrderedTasks(TaskList* order, int cookie)
      : order_(order),
        type_(ORDERERD),
        cookie_(cookie) {
  }
  OrderedTasks(TaskList* order, TaskType type, int cookie)
      : order_(order),
        type_(type),
        cookie_(cookie) {
  }

  void RunStart() {
    TaskItem item(type_, cookie_, true);
    DLOG(INFO) << item;
    order_->push_back(item);
  }
  void RunEnd() {
    TaskItem item(type_, cookie_, false);
    DLOG(INFO) << item;
    order_->push_back(item);
  }

  virtual void Run() {
    RunStart();
    RunEnd();
  }

 protected:
  TaskList* order() const {
    return order_;
  }

  int cookie() const {
    return cookie_;
  }

 private:
  TaskList* order_;
  TaskType type_;
  int cookie_;
};

#if defined(OS_WIN)

// MessageLoop implicitly start a "modal message loop". Modal dialog boxes,
// common controls (like OpenFile) and StartDoc printing function can cause
// implicit message loops.
class MessageBoxTask : public OrderedTasks {
 public:
  MessageBoxTask(TaskList* order, int cookie, bool is_reentrant)
      : OrderedTasks(order, MESSAGEBOX, cookie),
        is_reentrant_(is_reentrant) {
  }

  virtual void Run() {
    RunStart();
    if (is_reentrant_)
      MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageBox(NULL, L"Please wait...", kMessageBoxTitle, MB_OK);
    RunEnd();
  }

 private:
  bool is_reentrant_;
};

// Will end the MessageBox.
class EndDialogTask : public OrderedTasks {
 public:
  EndDialogTask(TaskList* order, int cookie)
      : OrderedTasks(order, ENDDIALOG, cookie) {
  }

  virtual void Run() {
    RunStart();
    HWND window = GetActiveWindow();
    if (window != NULL) {
      EXPECT_NE(EndDialog(window, IDCONTINUE), 0);
      // Cheap way to signal that the window wasn't found if RunEnd() isn't
      // called.
      RunEnd();
    }
  }
};

#endif  // defined(OS_WIN)

class RecursiveTask : public OrderedTasks {
 public:
  RecursiveTask(int depth, TaskList* order, int cookie, bool is_reentrant)
      : OrderedTasks(order, RECURSIVE, cookie),
        depth_(depth),
        is_reentrant_(is_reentrant) {
  }

  virtual void Run() {
    RunStart();
    if (depth_ > 0) {
      if (is_reentrant_)
        MessageLoop::current()->SetNestableTasksAllowed(true);
      MessageLoop::current()->PostTask(FROM_HERE,
          new RecursiveTask(depth_ - 1, order(), cookie(), is_reentrant_));
    }
    RunEnd();
  }

 private:
  int depth_;
  bool is_reentrant_;
};

class QuitTask : public OrderedTasks {
 public:
  QuitTask(TaskList* order, int cookie)
      : OrderedTasks(order, QUITMESSAGELOOP, cookie) {
  }

  virtual void Run() {
    RunStart();
    MessageLoop::current()->Quit();
    RunEnd();
  }
};

#if defined(OS_WIN)

class Recursive2Tasks : public Task {
 public:
  Recursive2Tasks(MessageLoop* target,
                  HANDLE event,
                  bool expect_window,
                  TaskList* order,
                  bool is_reentrant)
      : target_(target),
        event_(event),
        expect_window_(expect_window),
        order_(order),
        is_reentrant_(is_reentrant) {
  }

  virtual void Run() {
    target_->PostTask(FROM_HERE,
                      new RecursiveTask(2, order_, 1, is_reentrant_));
    target_->PostTask(FROM_HERE,
                      new MessageBoxTask(order_, 2, is_reentrant_));
    target_->PostTask(FROM_HERE,
                      new RecursiveTask(2, order_, 3, is_reentrant_));
    // The trick here is that for recursive task processing, this task will be
    // ran _inside_ the MessageBox message loop, dismissing the MessageBox
    // without a chance.
    // For non-recursive task processing, this will be executed _after_ the
    // MessageBox will have been dismissed by the code below, where
    // expect_window_ is true.
    target_->PostTask(FROM_HERE, new EndDialogTask(order_, 4));
    target_->PostTask(FROM_HERE, new QuitTask(order_, 5));

    // Enforce that every tasks are sent before starting to run the main thread
    // message loop.
    ASSERT_TRUE(SetEvent(event_));

    // Poll for the MessageBox. Don't do this at home! At the speed we do it,
    // you will never realize one MessageBox was shown.
    for (; expect_window_;) {
      HWND window = FindWindow(L"#32770", kMessageBoxTitle);
      if (window) {
        // Dismiss it.
        for (;;) {
          HWND button = FindWindowEx(window, NULL, L"Button", NULL);
          if (button != NULL) {
            EXPECT_TRUE(0 == SendMessage(button, WM_LBUTTONDOWN, 0, 0));
            EXPECT_TRUE(0 == SendMessage(button, WM_LBUTTONUP, 0, 0));
            break;
          }
        }
        break;
      }
    }
  }

 private:
  MessageLoop* target_;
  HANDLE event_;
  TaskList* order_;
  bool expect_window_;
  bool is_reentrant_;
};

#endif  // defined(OS_WIN)

void RunTest_RecursiveDenial1(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 1, false));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 2, false));
  MessageLoop::current()->PostTask(FROM_HERE, new QuitTask(&order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(14U, order.size());
  EXPECT_EQ(order[ 0], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[ 3], TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order[ 4], TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order[ 5], TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order[ 6], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 7], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 8], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[ 9], TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order[10], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[11], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[12], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[13], TaskItem(RECURSIVE, 2, false));
}

void RunTest_RecursiveSupport1(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);
  
  TaskList order;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 1, true));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 2, true));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new QuitTask(&order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(14U, order.size());
  EXPECT_EQ(order[ 0], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[ 3], TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order[ 4], TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order[ 5], TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order[ 6], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 7], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 8], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[ 9], TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order[10], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[11], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[12], TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order[13], TaskItem(RECURSIVE, 2, false));
}

#if defined(OS_WIN)
// TODO(darin): These tests need to be ported since they test critical
// message loop functionality.

// A side effect of this test is the generation a beep. Sorry.
void RunTest_RecursiveDenial2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveDenial2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.message_loop()->PostTask(FROM_HERE,
                                  new Recursive2Tasks(MessageLoop::current(),
                                                      event,
                                                      true,
                                                      &order,
                                                      false));
  // Let the other thread execute.
  WaitForSingleObject(event, INFINITE);
  MessageLoop::current()->Run();

  ASSERT_EQ(order.size(), 17);
  EXPECT_EQ(order[ 0], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(MESSAGEBOX, 2, true));
  EXPECT_EQ(order[ 3], TaskItem(MESSAGEBOX, 2, false));
  EXPECT_EQ(order[ 4], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[ 5], TaskItem(RECURSIVE, 3, false));
  // When EndDialogTask is processed, the window is already dismissed, hence no
  // "end" entry.
  EXPECT_EQ(order[ 6], TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order[ 7], TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order[ 8], TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order[ 9], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[10], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[11], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[12], TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order[13], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[14], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[15], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[16], TaskItem(RECURSIVE, 3, false));
}

// A side effect of this test is the generation a beep. Sorry.  This test also
// needs to process windows messages on the current thread.
void RunTest_RecursiveSupport2(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  Thread worker("RecursiveSupport2_worker");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_EQ(true, worker.StartWithOptions(options));
  TaskList order;
  ScopedHandle event(CreateEvent(NULL, FALSE, FALSE, NULL));
  worker.message_loop()->PostTask(FROM_HERE,
                                  new Recursive2Tasks(MessageLoop::current(),
                                                      event,
                                                      false,
                                                      &order,
                                                      true));
  // Let the other thread execute.
  WaitForSingleObject(event, INFINITE);
  MessageLoop::current()->Run();

  ASSERT_EQ(order.size(), 18);
  EXPECT_EQ(order[ 0], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(MESSAGEBOX, 2, true));
  // Note that this executes in the MessageBox modal loop.
  EXPECT_EQ(order[ 3], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[ 4], TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order[ 5], TaskItem(ENDDIALOG, 4, true));
  EXPECT_EQ(order[ 6], TaskItem(ENDDIALOG, 4, false));
  EXPECT_EQ(order[ 7], TaskItem(MESSAGEBOX, 2, false));
  /* The order can subtly change here. The reason is that when RecursiveTask(1)
     is called in the main thread, if it is faster than getting to the
     PostTask(FROM_HERE, QuitTask) execution, the order of task execution can
     change. We don't care anyway that the order isn't correct.
  EXPECT_EQ(order[ 8], TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order[ 9], TaskItem(QUITMESSAGELOOP, 5, false));
  EXPECT_EQ(order[10], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[11], TaskItem(RECURSIVE, 1, false));
  */
  EXPECT_EQ(order[12], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[13], TaskItem(RECURSIVE, 3, false));
  EXPECT_EQ(order[14], TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order[15], TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order[16], TaskItem(RECURSIVE, 3, true));
  EXPECT_EQ(order[17], TaskItem(RECURSIVE, 3, false));
}

#endif  // defined(OS_WIN)

class TaskThatPumps : public OrderedTasks {
 public:
  TaskThatPumps(TaskList* order, int cookie)
      : OrderedTasks(order, PUMPS, cookie) {
  }

  virtual void Run() {
    RunStart();
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->RunAllPending();
    MessageLoop::current()->SetNestableTasksAllowed(old_state);
    RunEnd();
  }
};

// Tests that non nestable tasks run in FIFO if there are no nested loops.
void RunTest_NonNestableWithNoNesting(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  Task* task = new OrderedTasks(&order, 1);
  MessageLoop::current()->PostNonNestableTask(FROM_HERE, task);
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 2));
  MessageLoop::current()->PostTask(FROM_HERE, new QuitTask(&order, 3));
  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(6U, order.size());
  EXPECT_EQ(order[ 0], TaskItem(ORDERERD, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(ORDERERD, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(ORDERERD, 2, true));
  EXPECT_EQ(order[ 3], TaskItem(ORDERERD, 2, false));
  EXPECT_EQ(order[ 4], TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order[ 5], TaskItem(QUITMESSAGELOOP, 3, false));
}

// Tests that non nestable tasks don't run when there's code in the call stack.
void RunTest_NonNestableInNestedLoop(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  TaskList order;

  MessageLoop::current()->PostTask(FROM_HERE,
                                   new TaskThatPumps(&order, 1));
  Task* task = new OrderedTasks(&order, 2);
  MessageLoop::current()->PostNonNestableTask(FROM_HERE, task);
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 3));
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 4));
  Task* non_nestable_quit = new QuitTask(&order, 5);
  MessageLoop::current()->PostNonNestableTask(FROM_HERE, non_nestable_quit);

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(10U, order.size());
  EXPECT_EQ(order[ 0], TaskItem(PUMPS, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(ORDERERD, 3, true));
  EXPECT_EQ(order[ 2], TaskItem(ORDERERD, 3, false));
  EXPECT_EQ(order[ 3], TaskItem(ORDERERD, 4, true));
  EXPECT_EQ(order[ 4], TaskItem(ORDERERD, 4, false));
  EXPECT_EQ(order[ 5], TaskItem(PUMPS, 1, false));
  EXPECT_EQ(order[ 6], TaskItem(ORDERERD, 2, true));
  EXPECT_EQ(order[ 7], TaskItem(ORDERERD, 2, false));
  EXPECT_EQ(order[ 8], TaskItem(QUITMESSAGELOOP, 5, true));
  EXPECT_EQ(order[ 9], TaskItem(QUITMESSAGELOOP, 5, false));
}

#if defined(OS_WIN)

class AutoresetWatcher : public MessageLoopForIO::Watcher {
 public:
  AutoresetWatcher(HANDLE signal) : signal_(signal) {
  }
  virtual void OnObjectSignaled(HANDLE object);
 private:
  HANDLE signal_;
};

void AutoresetWatcher::OnObjectSignaled(HANDLE object) {
  MessageLoopForIO::current()->WatchObject(object, NULL);
  ASSERT_TRUE(SetEvent(signal_));
}

class AutoresetTask : public Task {
 public:
  AutoresetTask(HANDLE object, MessageLoopForIO::Watcher* watcher)
    : object_(object), watcher_(watcher) {}
  virtual void Run() {
    MessageLoopForIO::current()->WatchObject(object_, watcher_);
  }

 private:
  HANDLE object_;
  MessageLoopForIO::Watcher* watcher_;
};

void RunTest_AutoresetEvents(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  SECURITY_ATTRIBUTES attributes;
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = false;
  attributes.lpSecurityDescriptor = NULL;

  // Init an autoreset and a manual reset events.
  HANDLE autoreset = CreateEvent(&attributes, FALSE, FALSE, NULL);
  HANDLE callback_called = CreateEvent(&attributes, TRUE, FALSE, NULL);
  ASSERT_TRUE(NULL != autoreset);
  ASSERT_TRUE(NULL != callback_called);

  Thread thread("Autoreset test");
  Thread::Options options;
  options.message_loop_type = message_loop_type;
  ASSERT_TRUE(thread.StartWithOptions(options));

  MessageLoop* thread_loop = thread.message_loop();
  ASSERT_TRUE(NULL != thread_loop);

  AutoresetWatcher watcher(callback_called);
  AutoresetTask* task = new AutoresetTask(autoreset, &watcher);
  thread_loop->PostTask(FROM_HERE, task);
  Sleep(100);  // Make sure the thread runs and sleeps for lack of work.

  ASSERT_TRUE(SetEvent(autoreset));

  DWORD result = WaitForSingleObject(callback_called, 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

class DispatcherImpl : public MessageLoopForUI::Dispatcher {
 public:
  DispatcherImpl() : dispatch_count_(0) {}

  virtual bool Dispatch(const MSG& msg) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    return (++dispatch_count_ != 2);
  }

  int dispatch_count_;
};

void RunTest_Dispatcher(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  class MyTask : public Task {
  public:
    virtual void Run() {
      PostMessage(NULL, WM_LBUTTONDOWN, 0, 0);
      PostMessage(NULL, WM_LBUTTONUP, 'A', 0);
    }
  };
  Task* task = new MyTask();
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, 100);
  DispatcherImpl dispatcher;
  MessageLoopForUI::current()->Run(&dispatcher);
  ASSERT_EQ(2, dispatcher.dispatch_count_);
}

#endif  // defined(OS_WIN)

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that message loops work properly in all configurations.  Of course, in some
// cases, a unit test may only be for a particular type of loop.

TEST(MessageLoopTest, PostTask) {
  RunTest_PostTask(MessageLoop::TYPE_DEFAULT);
  RunTest_PostTask(MessageLoop::TYPE_UI);
  RunTest_PostTask(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostTask_SEH) {
  RunTest_PostTask_SEH(MessageLoop::TYPE_DEFAULT);
  RunTest_PostTask_SEH(MessageLoop::TYPE_UI);
  RunTest_PostTask_SEH(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_Basic) {
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_Basic(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InDelayOrder) {
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InDelayOrder(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder) {
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder_2) {
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder_2(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, PostDelayedTask_InPostOrder_3) {
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_DEFAULT);
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_UI);
  RunTest_PostDelayedTask_InPostOrder_3(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
TEST(MessageLoopTest, Crasher) {
  RunTest_Crasher(MessageLoop::TYPE_DEFAULT);
  RunTest_Crasher(MessageLoop::TYPE_UI);
  RunTest_Crasher(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, CrasherNasty) {
  RunTest_CrasherNasty(MessageLoop::TYPE_DEFAULT);
  RunTest_CrasherNasty(MessageLoop::TYPE_UI);
  RunTest_CrasherNasty(MessageLoop::TYPE_IO);
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, Nesting) {
  RunTest_Nesting(MessageLoop::TYPE_DEFAULT);
  RunTest_Nesting(MessageLoop::TYPE_UI);
  RunTest_Nesting(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveDenial1) {
  RunTest_RecursiveDenial1(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial1(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial1(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveSupport1) {
  RunTest_RecursiveSupport1(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveSupport1(MessageLoop::TYPE_UI);
  RunTest_RecursiveSupport1(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
TEST(MessageLoopTest, RecursiveDenial2) {
  RunTest_RecursiveDenial2(MessageLoop::TYPE_DEFAULT);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_UI);
  RunTest_RecursiveDenial2(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, RecursiveSupport2) {
  // This test requires a UI loop
  RunTest_RecursiveSupport2(MessageLoop::TYPE_UI);
}
#endif  // defined(OS_WIN)

TEST(MessageLoopTest, NonNestableWithNoNesting) {
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_DEFAULT);
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_UI);
  RunTest_NonNestableWithNoNesting(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, NonNestableInNestedLoop) {
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_DEFAULT);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_UI);
  RunTest_NonNestableInNestedLoop(MessageLoop::TYPE_IO);
}

#if defined(OS_WIN)
TEST(MessageLoopTest, AutoresetEvents) {
  // This test requires an IO loop
  RunTest_AutoresetEvents(MessageLoop::TYPE_IO);
}

TEST(MessageLoopTest, Dispatcher) {
  // This test requires a UI loop
  RunTest_Dispatcher(MessageLoop::TYPE_UI);
}
#endif  // defined(OS_WIN)
