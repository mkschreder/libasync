Asynchronous Programming in C and C++ 
-------------------------------------

The C and C++ languages have been originally designed during a time when people have not had much grasp on asynchronous patterns. It is only recently that asynchronous patterns have become more mainstream with the advent of languages that support asynchronous programming natively. Web developers in particular have become very accustomed with asynchronous patterns because all of web browser scripting is asynchronous.

But in the C and C++ world we have been lagging behind. There has not been a technique around that would allow for coding patterns where methods would take several frames to complete. This is not at all the same as threads - it is a whole new way of writing code. In fact, we can't even implement this with threads because it is quite a different concept. 

What we want is ability to do this: 
	
	async method(argument){
		sleep(100); // async sleep 
		...
		printf("%s\n", argument); 
	}
	await method("test"); 
	printf("blah\n"); 
	
and the result should be this: 
	blah
	.. delay 
	test
	
See the difference? We want the async method to never block. This technique is extremely useful for complex processes inside an application that actually need to sleep and then continue after a delay but it should not halt the whole application - only the method that sleeps. 

We can implement this if only we could have a technique to pause a method and then resume it. Luckily we have such a technique, in fact two of them: one is switch statements that can jump arbitrarily even into loops, the other is address labels which are even better because they can jump anywhere inside a function without requiring the function to start from the top. 

One important side effect of async methods is that they can not have any local variables - and in fact, they should not! You should instead always declare all variables that are used inside async methods outside of them - for example inside a structure which the async method is a member of. 

Async processes and methods
---------------------------

There are two main types of constructs in the async library: the processes and the methods. Processes are more heavy weight because they also maintain data structures to allow them to be chained into lists of tasks. Methods on the other hand are extremely lightweight. 

Processes can be stacked into queues. 

	// initialize a process structure
	ASYNC_PROCESS_INIT(&process_struct, process_method); 
	// link process structure to a queue
	ASYNC_QUEUE_WORK(&process_queue, &process_struct); 
	..
	// run the queue once
	ASYNC_RUN_PARALLEL(&process_queue); 
	..
	// normally you would do this though: 
	AWAIT(ASYNC_RUN_PARALLEL(&process_queue)); 
	// .. or this: 
	AWAIT(ASYNC_RUN_SERIES(&process_queue)); 
	// both of which asynchronously wait for all processes 
	// to complete before your code continues. 
	
Methods are lightweight and can not be stacked - but you can have many of them because they are light-weight. 

Both processes and threads must always contain following two statements: 
	
	ASYNC_BEGIN(); 
	// and 
	ASYNC_END(return_value); 

C processes are defined like this: 
	
	ASYNC_PROCESS(process_name){
		ASYNC_BEGIN(); 
		// ... code
		ASYNC_END(0); // processes always return int
	}

.. and used like this: 
	
	ASYNC_QUEUE(queue); 
	struct async_process proc; 
	
	ASYNC_PROCESS_INIT(&proc, process_name); 
	ASYNC_QUEUE_WORK(&queue, &proc); 
	
C async methods are defined like this: 

	// prototypes can be defined like this: 
	ASYNC_PROTOTYPE(const char*, mystruct_t, method_name, 
		char *arg, int arg2); 
	// implementations like this: 
	ASYNC(const char*, mystruct_t, method_name, 
		char *arg, int arg2){
		ASYNC_BEGIN(); 
		ASYNC_END("result string"); 
	}

First argument is return type, second is struct which contains data for this method. Third is the method name and then you can write all the arguments. 

You can then define you struct like this: 

	struct mystruct {
		// this is data - must be same name as the method above. 
		struct async_task method_name; 
		struct async_process process; 
		// some other struct data 
		const char *str; 
	}; 
	// typedef because only single words can be used in C macros
	typedef struct mystruct mystruct_t; 
	ASYNC(int, mystruct_t, method_name){
		// async methods always get __self parameter which points to 
		// structure of type mystruct_t (in this case) 
		struct mystruct *self = __self; 
		ASYNC_BEGIN(); 
		// use the struct data
		printf("%s\n", self->str); 
		ASYNC_END(0); 
	}

	ASYNC_PROCESS(mystruct_process){
		// retreive the struct itself 
		struct mystruct *self = container_of(__self, struct mystruct, process); 
		ASYNC_BEGIN(); 
		// wait for our task. It takes parameter of type mystruct_t*
		AWAIT_TASK(int, mystruct_t, method_name, self); 
		ASYNC_END(0); 
	}

	void mystruct_init(struct mystruct *self, const char *str){
		ASYNC_INIT(&self->method_name); 
		ASYNC_PROCESS_INIT(&self->process, mystruct_process); 
		ASYNC_QUEUE_WORK(&ASYNC_GLOBAL_QUEUE, &self->process); 
		self->str = str; 
	}

	int main(){
		struct mystruct my_struct; 
		mystruct_init(&my_struct, "Hello World!\n"); 
		
		// run the queue until all tasks are done
		while(ASYNC_RUN_PARALLEL(&ASYNC_GLOBAL_QUEUE)); 
		
		return 0; 
	}
	
Properties of async methods: 
	
* They have built in protection against being called from more than one other async method at a time (it is usually the case that you want an async method to complete before another caller can call it)
* They can not have local variables (due to jumps inside the method which will cause undefined behaviour if local variabels are present between ASYNC_BEGIN and ASYNC_END
* They can take multiple calls to complete (just use AWAIT_TASK() to asynchronously wait for a task to complete). 

Locking and types of protection
-------------------------------

Even though async methods are all running inside the same thread, the main program thread, we still encounter similar problems with concurrent data access as we do with normal threads. The only difference is that our concurrency problems only occur if thread needs to ensure that data does not change once it yields and then is resumed again. 

Async includes several built in methods for handling this: 

* Async mutex: declare async_mutex_t, init it using ASYNC_MUTEX_INIT(&mutex, 1); Then use it in a task like this: ASYNC_MUTEX_LOCK(&mutex); 
* Keeping track of parent: all tasks get a parameter called parent which points to the struct async_task of the calling task. You can save this parameter in your structure and use it to limit access to only one caller at a time. 

Stackless threads have been chosen because they are the most lightweight kind of threads available and they have very small overhead. There are however both advantages and also some disadvantages to using stackless protothreads. 

Advantages of stackless protothreads: 

- Allow device drivers to be written such that no CPU cycles are ever wasted waiting for I/O. The device driver can simply return control to the application and wait until the next time it has the chance to run. 
- Writing asynchronous tasks becomes a lot easier because they can be written linearly instead of being organized as a complicated state machine. 
- No thread is ever interrupted while it is in the middle of some operation. Threads always run to completion. Meaning that we can design our code without having to think about byte level synchronization (device level syncrhonization is required though). 
- No stack also means that we never run out of stack space inside a thread. 
- Context switching is very fast - current thread method saves it's resume point, returns to libk scheduler, libk scheduler loads the address of the next thread function to run, and calls it. 
- Not a problem to have tens of threads for each asynchronous action. Since threads are just normal methods minus the stack, we can have many threads without experiencing significant slowdown. 
- Synchronization is much easier because all code that you "see" is atomic until it explicitly releases control to the scheduler. We thus do not have to worry about non atomic memory access. 

Disadvantages of this approach: 

- Stackless means that no variable on the thread method stack is valid after a thread returns and then resumes again. Although we can easily solve this by maintaining the context inside a separate object to which the thread is attached. 
- No preemption also means that a thread can keep CPU to itself for as long as it wishes. It is up to the programmer to release the CPU as quickly as possible. Since device drivers usually use interrupt requests to respond in realtime anyway, this limitation has not proven to be a problem so far. 
- No thread can ever call another thread or spawn a new thread. Threads are only single level. All other code called from inside the thread can be considered to execute atomically (except for when it is interrupted by an ISR). 
- Data that needs to be saved across multiple thread switches must be stored in memory (this is a problem with all multitasking though). 
- Longer response times for tasks - a task can lose cpu for as long as it takes the heavies task to finish. This is solved by programmer explicitly designing threads such that control is periodically released to the scheduler. 

