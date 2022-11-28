/* SPDX-License-Identifier: BSD-2-Clause */

#include <tilck_gen_headers/config_debug.h>

#include <tilck/common/basic_defs.h>
#include <tilck/common/printk.h>
#include <tilck/common/utils.h>

#include <tilck/kernel/process.h>
#include <tilck/kernel/sync.h>
#include <tilck/kernel/debug_utils.h>
#include <tilck/kernel/self_tests.h>
#include <tilck/kernel/timer.h>


static struct ksem test_sem;

enum sem_test_state {

   NOT_STARTED,
   BLOCKED,
   DONE,
};

struct sem_test_data {

   int units;
   ATOMIC(enum sem_test_state) state;
   int tid;
};

static struct sem_test_data sem_test_waiters[] =
{
   { 1, NOT_STARTED, -1 },
   { 3, NOT_STARTED, -1 },
   { 5, NOT_STARTED, -1 },
   { 10, NOT_STARTED, -1 },
};




#define TABLE_SIZE 6
static struct ksem seat_sem;
static int seat;
static unsigned char table[TABLE_SIZE];
static struct ksem eatingSemaphores[TABLE_SIZE];

static void sitAndEat()
{
   ksem_wait(&seat_sem, 1, -1);
   int local_seat = seat;
   seat++;
   printk("Grabbing seat %d\n",local_seat);
   ksem_signal(&seat_sem,1);


   struct ksem *left = &eatingSemaphores[local_seat];
   struct ksem *right = &eatingSemaphores[(local_seat+1)%TABLE_SIZE];

   while(1) {
      switch(local_seat%2)
      {
         case 0:
            printk("Starting to wait %d\n",local_seat);
            ksem_wait(left,1,-1);
            ksem_wait(right,1,-1);
            break;
         case 1:
            printk("Starting to wait %d\n",local_seat);
            ksem_wait(right,1,-1);
            ksem_wait(left,1,-1);
            break;
      }
      printk("Seat number: %d taken of %d\n",local_seat,seat);
      table[local_seat] = 'E';
      int j = 0;
      while(j<TABLE_SIZE)
         printk("%c ", table[j++]);
      printk("\n");
      int i = 0;
      while(i<100000000) i++;
      table[local_seat] = '-';
      switch(local_seat%2)
      {
         case 0:
            ksem_signal(left,1);
            ksem_signal(right,1);
            break;
         case 1:
            ksem_signal(right,1);
            ksem_signal(left,1);
            break;
      }
      printk("Dropped both sems %d\n", local_seat);
      while(i<1000000000) i++;
   }
}

static int philo()
{
   seat=0;
   ksem_init(&seat_sem, 0, 1000);

   int i = 0;
   while(i< TABLE_SIZE){
      table[i] = '-';
      ksem_init(&eatingSemaphores[i++], 1, 1000);
   }

   i = 0;
   while(i < TABLE_SIZE){
      if(kthread_create(&sitAndEat, 0, NULL)<0)
         panic("Unable to create a thread for sitAndEat()");
      i++;
   }
   printk("philo finished\n");
   ksem_signal(&seat_sem,1);

   while(true) i++;

   return 0;
}

static void ksem_test_wait_thread(void *arg)
{
   struct sem_test_data *ctx = arg;
   int rc;

   printk("ksem wait thread %d, wait: %d\n", get_curr_tid(), ctx->units);

   ctx->state = BLOCKED;
   rc = ksem_wait(&test_sem, ctx->units, KSEM_WAIT_FOREVER);

   if (rc != 0) {
      panic("ksem_wait() failed with: %d\n", rc);
   }

   printk("wait(%d) done, rem_counter: %d\n", ctx->units, test_sem.counter);
   ctx->state = DONE;
}

static void wait_for_all_blocked(void)
{
   while (true) {

      bool all_blocked = true;

      for (u32 i = 0; i < ARRAY_SIZE(sem_test_waiters); i++) {
         if (sem_test_waiters[i].state != BLOCKED) {
            all_blocked = false;
            break;
         }
      }

      if (all_blocked)
         break;

      kernel_yield();
   }
}

static void
ksem_test_check_sem_counter(int expected)
{
   disable_preemption();

   if (test_sem.counter == expected)
      goto out;

   printk("FAIL: counter: %d != expected: %d\n", test_sem.counter, expected);

   for (u32 i = 0; i < ARRAY_SIZE(sem_test_waiters); i++) {

      struct task *task = get_task(sem_test_waiters[i].tid);

      printk("waiter[%u, tid: %d]: units: %d, state: %d, task state: %s\n",
             i, sem_test_waiters[i].tid,
             sem_test_waiters[i].units, sem_test_waiters[i].state,
             task ? task_state_str[task->state] : "N/A");
   }

   panic("ksem test failure");

out:
   enable_preemption();
}

void selftest_ksem()
{
   int tid, rc;
   ksem_init(&test_sem, 0, 1000);
   philo();
   return;
   printk("Running the wait threads...\n");
   disable_preemption();
   {
      for (u32 i = 0; i < ARRAY_SIZE(sem_test_waiters); i++) {
         tid = kthread_create(ksem_test_wait_thread, 0, &sem_test_waiters[i]);
         VERIFY(tid > 0);
         sem_test_waiters[i].tid = tid;
      }
   }
   printk("Waiting for them to block...\n");
   enable_preemption();
   wait_for_all_blocked();

   VERIFY(sem_test_waiters[0].state == BLOCKED);
   VERIFY(sem_test_waiters[1].state == BLOCKED);
   VERIFY(sem_test_waiters[2].state == BLOCKED);
   VERIFY(sem_test_waiters[3].state == BLOCKED);
   ksem_test_check_sem_counter(0);

   printk("signal(2)\n");
   rc = ksem_signal(&test_sem, 2);
   VERIFY(rc == 0);
   yield_until_last();

   ksem_test_check_sem_counter(1);
   VERIFY(sem_test_waiters[0].state == DONE);
   VERIFY(sem_test_waiters[1].state == BLOCKED);
   VERIFY(sem_test_waiters[2].state == BLOCKED);
   VERIFY(sem_test_waiters[3].state == BLOCKED);

   printk("signal(9)\n");
   rc = ksem_signal(&test_sem, 9);
   VERIFY(rc == 0);
   yield_until_last();

   ksem_test_check_sem_counter(2);
   VERIFY(sem_test_waiters[0].state == DONE);
   VERIFY(sem_test_waiters[1].state == DONE);
   VERIFY(sem_test_waiters[2].state == DONE);
   VERIFY(sem_test_waiters[3].state == BLOCKED);

   printk("signal(5)\n");
   rc = ksem_signal(&test_sem, 5);
   VERIFY(rc == 0);
   yield_until_last();

   ksem_test_check_sem_counter(2+5);
   VERIFY(sem_test_waiters[0].state == DONE);
   VERIFY(sem_test_waiters[1].state == DONE);
   VERIFY(sem_test_waiters[2].state == DONE);
   VERIFY(sem_test_waiters[3].state == BLOCKED);

   printk("signal(3)\n");
   rc = ksem_signal(&test_sem, 3);
   VERIFY(rc == 0);
   yield_until_last();

   ksem_test_check_sem_counter(0);
   VERIFY(sem_test_waiters[0].state == DONE);
   VERIFY(sem_test_waiters[1].state == DONE);
   VERIFY(sem_test_waiters[2].state == DONE);
   VERIFY(sem_test_waiters[3].state == DONE);

   printk("Done\n");
   ksem_destroy(&test_sem);

   if (se_is_stop_requested())
      se_interrupted_end();
   else
      se_regular_end();
}

REGISTER_SELF_TEST(ksem, se_short, &selftest_ksem)

