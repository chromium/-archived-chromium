// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_handle.h"
#include "base/thread.h"
#include "base/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MessageLoopTest : public testing::Test {
 public:
  virtual void SetUp() {
    enable_recursive_task_ = MessageLoop::current()->NestableTasksAllowed();
  }
  virtual void TearDown() {
    MessageLoop::current()->SetNestableTasksAllowed(enable_recursive_task_);
  }
 private:
  bool enable_recursive_task_;
};

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

}  // namespace

TEST(MessageLoopTest, PostTask) {
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

TEST(MessageLoopTest, InvokeLater_SEH) {
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

namespace {

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
    Sleep(1);
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

#elif

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

}  // namespace


TEST(MessageLoopTest, Crasher) {
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


TEST(MessageLoopTest, CrasherNasty) {
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


TEST(MessageLoopTest, Nesting) {
  int depth = 100;
  MessageLoop::current()->PostTask(FROM_HERE, new NestingTest(&depth));
  MessageLoop::current()->Run();
  EXPECT_EQ(depth, 0);
}

namespace {

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

}  // namespace

TEST(MessageLoop, RecursiveDenial1) {
  EXPECT_TRUE(MessageLoop::current()->NestableTasksAllowed());
  TaskList order;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 1, false));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 2, false));
  MessageLoop::current()->PostTask(FROM_HERE, new QuitTask(&order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(order.size(), 14);
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


TEST(MessageLoop, RecursiveSupport1) {
  TaskList order;
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 1, true));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RecursiveTask(2, &order, 2, true));
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new QuitTask(&order, 3));

  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(order.size(), 14);
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

// A side effect of this test is the generation a beep. Sorry.
TEST(MessageLoop, RecursiveDenial2) {
  Thread worker("RecursiveDenial2_worker");
  ASSERT_EQ(true, worker.Start());
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

// A side effect of this test is the generation a beep. Sorry.
TEST(MessageLoop, RecursiveSupport2) {
  Thread worker("RecursiveSupport2_worker");
  ASSERT_EQ(true, worker.Start());
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

class TaskThatPumps : public OrderedTasks {
 public:
  TaskThatPumps(TaskList* order, int cookie)
      : OrderedTasks(order, PUMPS, cookie) {
  }

  virtual void Run() {
    RunStart();
    bool old_state = MessageLoop::current()->NestableTasksAllowed();
    MessageLoop::current()->Quit();
    MessageLoop::current()->SetNestableTasksAllowed(true);
    MessageLoop::current()->Run();
    MessageLoop::current()->SetNestableTasksAllowed(old_state);
    RunEnd();
  }

 private:
};


// Tests that non nestable tasks run in FIFO if there are no nested loops.
TEST(MessageLoop, NonNestableWithNoNesting) {
  TaskList order;

  Task* task = new OrderedTasks(&order, 1);
  task->set_nestable(false);
  MessageLoop::current()->PostTask(FROM_HERE, task);
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 2));
  MessageLoop::current()->PostTask(FROM_HERE, new QuitTask(&order, 3));
  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(order.size(), 6);
  EXPECT_EQ(order[ 0], TaskItem(ORDERERD, 1, true));
  EXPECT_EQ(order[ 1], TaskItem(ORDERERD, 1, false));
  EXPECT_EQ(order[ 2], TaskItem(ORDERERD, 2, true));
  EXPECT_EQ(order[ 3], TaskItem(ORDERERD, 2, false));
  EXPECT_EQ(order[ 4], TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order[ 5], TaskItem(QUITMESSAGELOOP, 3, false));
}

// Tests that non nestable tasks don't run when there's code in the call stack.
TEST(MessageLoop, NonNestableInNestedLoop) {
  TaskList order;

  MessageLoop::current()->PostTask(FROM_HERE,
                                   new TaskThatPumps(&order, 1));
  Task* task = new OrderedTasks(&order, 2);
  task->set_nestable(false);
  MessageLoop::current()->PostTask(FROM_HERE, task);
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 3));
  MessageLoop::current()->PostTask(FROM_HERE, new OrderedTasks(&order, 4));
  Task* non_nestable_quit = new QuitTask(&order, 5);
  non_nestable_quit->set_nestable(false);
  MessageLoop::current()->PostTask(FROM_HERE, non_nestable_quit);


  MessageLoop::current()->Run();

  // FIFO order.
  ASSERT_EQ(order.size(), 10);
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


namespace {

class AutoresetWatcher : public MessageLoop::Watcher {
 public:
  AutoresetWatcher(HANDLE signal, MessageLoop* message_loop)
      : signal_(signal), message_loop_(message_loop) {}
  virtual void OnObjectSignaled(HANDLE object);
 private:
  HANDLE signal_;
  MessageLoop* message_loop_;
};

void AutoresetWatcher::OnObjectSignaled(HANDLE object) {
  message_loop_->WatchObject(object, NULL);
  ASSERT_TRUE(SetEvent(signal_));
}

class AutoresetTask : public Task {
 public:
  AutoresetTask(HANDLE object, MessageLoop::Watcher* watcher)
    : object_(object), watcher_(watcher) {}
  virtual void Run() {
    MessageLoop::current()->WatchObject(object_, watcher_);
  }

 private:
  HANDLE object_;
  MessageLoop::Watcher* watcher_;
};

}  // namespace

TEST(MessageLoop, AutoresetEvents) {
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
  ASSERT_TRUE(thread.Start());

  MessageLoop* message_loop = thread.message_loop();
  ASSERT_TRUE(NULL != message_loop);

  AutoresetWatcher watcher(callback_called, message_loop);
  AutoresetTask* task = new AutoresetTask(autoreset, &watcher);
  message_loop->PostTask(FROM_HERE, task);
  Sleep(100);  // Make sure the thread runs and sleeps for lack of work.

  ASSERT_TRUE(SetEvent(autoreset));

  DWORD result = WaitForSingleObject(callback_called, 1000);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  thread.Stop();
}

namespace {

class DispatcherImpl : public MessageLoop::Dispatcher {
 public:
  DispatcherImpl() : dispatch_count_(0) {}

  virtual bool Dispatch(const MSG& msg) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    return (++dispatch_count_ != 2);
  }

  int dispatch_count_;
};

}  // namespace

TEST(MessageLoop, Dispatcher) {
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
  MessageLoop::current()->Run(&dispatcher);
  ASSERT_EQ(2, dispatcher.dispatch_count_);
}
