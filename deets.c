
void print_deets()
{
  acquire(&ptable.lock);
  for(struct proc* p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(!p || p->pid<3 ) continue;
    cprintf("ToRead %d %d %d \n",ticks,p->pid,p->curr_queue);
  }
  release(&ptable.lock);
}
