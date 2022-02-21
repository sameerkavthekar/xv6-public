#include "types.h"
#include "stat.h"
#include "user.h"
#include "info.h"

int
main(int args, char *argv[])
{
	struct proc_info p;
	p.forked = p.keyboard = p.runnable = p.scheduled = p.sleeping = p.traps = p.zombie = 0;
	int res = info(&p);
	if(res < 0)
		printf(1, "Failed\n");

	printf(1, "ZOMBIE      %d\n", p.zombie);
	printf(1, "RUNNABLE      %d\n", p.runnable);
	printf(1, "SLEEPING      %d\n", p.sleeping);
	printf(1, "TOTAL FORKS      %d\n", p.forked);
	printf(1, "TOTAL TRAPS      %d\n", p.traps);
	printf(1, "KEYBOARD INTERRUPTS      %d\n", p.keyboard);
	printf(1, "TOTAL SCHEDULED      %d\n", p.scheduled);
}