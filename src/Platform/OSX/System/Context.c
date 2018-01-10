// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>
#include "Context.h"

void
makecontext(uctx *ucp, void (*func)(void), intptr_t arg)
{
  long *sp;
  
  memset(&ucp->uc_mcontext, 0, sizeof ucp->uc_mcontext);
  ucp->uc_mcontext.mc_rdi = (long)arg;
  sp = (long*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/sizeof(long);
  sp -= 1;
  sp = (void*)((uintptr_t)sp - (uintptr_t)sp%16);	/* 16-align for OS X */
  *--sp = 0;	/* return address */
  ucp->uc_mcontext.mc_rip = (long)func;
  ucp->uc_mcontext.mc_rsp = (long)sp;
}

int
swapcontext(uctx *oucp, const uctx *ucp)
{
  if(getcontext(oucp) == 0)
    setcontext(ucp);
  return 0;
}
