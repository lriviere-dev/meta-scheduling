# meta-scheduling
Meta two(?) stage solutions framework for scheduling problems under uncertainty. 
"Meta-solutions" refer to "solutions" to scheduling problems that represent more than a single schedule solution. 
It is often the case that a solution given to a scheduling problem is a sequence of tasks, this kind of solutions are already "meta", as they represent all schedules where the tasks are in this specific order.
In this project however, "meta-solutions" can also represent several different sequences.

### File contents :
- MetaSolutions : Defines the MetaSolution virtual class, and several specific meta-solution classes ( GroupMetaSolution , SequenceMetaSolution ...)
- ListMetaSolutions : Defines the ListMetaSolution template class, which are a MetaSolution subclass. Lists of meta-solutions are a family of meta solutions that share specific properties, hence the subclass (But it would be possible to define each listof<T> class directly under MetaSolution). 
- Algorithms : Defines the virtual Algorithms class. Algorithms in this projet refer to decision algorithms used to compute solutions to problem. They Require a Policy to guide them.
- Policy : Defines the virtual Policy class. Also defines the policies used in this project (FIFO). Policies are used to find out which solution is extracted from a Meta solution for a given scenario. It is necessary to score the meta solution itself.
- Instance : Defines the instance reading classes and functions.
- Sequence : defines the Sequence class.
- Schedule : defines the Schedule class.
