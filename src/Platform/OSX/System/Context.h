// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#define	setcontext(u)	setmcontext(&(u)->uc_mcontext)
#define	getcontext(u)	getmcontext(&(u)->uc_mcontext)

#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>

typedef struct mcontext mctx;
typedef struct ucontext uctx;

extern	int		swapcontext(uctx*, const uctx*);
extern	void		makecontext(uctx*, void(*)(void), intptr_t);
extern	int		getmcontext(mctx*);
extern	void		setmcontext(const mctx*);

struct mcontext {
  /*
   * The first 20 fields must match the definition of
   * sigcontext. So that we can support sigcontext
   * and ucontext_t at the same time.
   */
  long	mc_onstack;		/* XXX - sigcontext compat. */
  long	mc_rdi;			/* machine state (struct trapframe) */
  long	mc_rsi;
  long	mc_rdx;
  long	mc_rcx;
  long	mc_r8;
  long	mc_r9;
  long	mc_rax;
  long	mc_rbx;
  long	mc_rbp;
  long	mc_r10;
  long	mc_r11;
  long	mc_r12;
  long	mc_r13;
  long	mc_r14;
  long	mc_r15;
  long	mc_trapno;
  long	mc_addr;
  long	mc_flags;
  long	mc_err;
  long	mc_rip;
  long	mc_cs;
  long	mc_rflags;
  long	mc_rsp;
  long	mc_ss;
  
  long	mc_len;			/* sizeof(mcontext_t) */
#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_XMM		0x10002
  long	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
  long	mc_ownedfp;
  /*
   * See <machine/fpu.h> for the internals of mc_fpstate[].
   */
  long	mc_fpstate[64];
  long	mc_spare[8];
};

struct ucontext {
  /*
   * Keep the order of the first two fields. Also,
   * keep them the first two fields in the structure.
   * This way we can have a union with struct
   * sigcontext and ucontext_t. This allows us to
   * support them both at the same time.
   * note: the union is not defined, though.
   */
  sigset_t	uc_sigmask;
  mctx	uc_mcontext;
  
  struct __ucontext *uc_link;
  stack_t		uc_stack;
  int		__spare__[8];
};
  
#ifdef __cplusplus
}
#endif
