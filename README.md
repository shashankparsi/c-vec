# Vec: A Dynamic Vector in C

The C-standard library doesn't offer a good dynamic data-structure often found
in other languages such a a `vector` in C++ or a `Vec` in Rust. This is an
overview of the idea, design and implementation of such data-structures in C.

## Features of a Dynamic Data-Structure

When we talk about a dynamic data-structure, we usually mean a container with a
growing memory buffer. Such buffer has a certain strategy to allocate more
memory when the existing memory runs out. Usually the startegy is to double the
size of the buffer each time a limit is reached. This way we minimize the number
of reallocations in the buffer which is our most expensive operation. While we
still have empty buffer left we can append to the buffer with negligible cost.
When we reach the limit and reallocate, we incur the cost of the allocation and
copying over the old buffer to the new buffer.

Such data-structures can be typed, but in this tutorial we will make our best to
mimic the usefullness of vectors in other languages, by providing a
generic interface which will accept any type of element.

## Dynamic Vector

Now let's implement a dynamic array data-structure. Let's consider the
equivalent Rust called `Vec`. `Vec` implements tons of functions, but in essence
it's a growable and shrinkable buffer of elements `<T>` and the main operations
are `push` and `pop`. Pushing a new lement at the end of the buffer and
"popping" aka. taking the element from the data-structure and giving it to you
individually (this is an important detail actually). Furthermore we usually have
`get` method or we can access the elements using `[]` syntax.

It's important to note that there is a differnce in performance between the
operations. Adding to the end of the array and deleting from the end will always
be cheapest. Everything else will incur the penalty of copying data around.

However in various tests I've made, those penalties are actually quite small
compared to penalties that incur with different data-structures claiming to
solve this problem. Sure you can prepend to a linked list witout removing or
shuffling around elements, but in a dynamic array implementation you are dealing
with data that is all laid out sequantially in memory. This is important,
because modern processors are very fast when the requiared data can be found in
cache. In a linked list or other pointer-type list, the cache misses that incur
from pointer indirection far outweight any other gains in my experience.

### Design

First let's talk a little about design patterns. We should adopt conventions
that make our API easy to reason with and practices that make the code safe and
easy to debug.

1. Allocation and deallocation happen at the same level as the variable
   declaration.

This first pattern aids at avoiding unnecessary allocations leading to increased
performance as well as easier time with leaks. Let's see what it means.

```c

// This "constructor" returns an allocated pointer to a `t_foo`. But what if
// well didn't need a pointer, because we use t_foo in the caller's scope only?
t_foo *create_a_foo(int foosize);

// We could just return a `t_foo`, but now the problem is the error condition.
// You can't return a NULL if something goes wrong.
t_foo create_a_foo(int foosize);

// This is how we create a `t_foo` without allocating a pointer (if it wasn'tons
// already allocated) and we can return an integer repesenting some error condition.
int create_a_foo(t_foo *foo, int foosize);

// ...or if we want to be passing foo around directly as a parameter to
// another function, we can return a pointer to `t_foo`.
t_foo *create_a_foo(t_foo *foo, int foosize);

```

2. Parameter order is (dst, src, params, fpointers).

We always the destination before the source if a destination pointer or variable
is applicable.

```c

int foo(char *dst, char *src, int len, (*bar)(char *));

```

### Struct

We create a struct called `s_vec` and typedef it to `t_vec`.

```c

typedef struct s_vec
{
	uint8_t	*memory;	// Pointer to the first byte of allocated memory.
	size_t	elem_size;	// Size of a vector element in bytes.
	size_t	alloc_size;	// Total size of allocated bytes.
	size_t	len;		// Length of the active part of the vector assert
						// `elem_size` chunks.
}	t_vec;

```

When we access elements in the vector our bounds are 0 -> len. We might have
allocated more memory in total, but we will only access memory in the byte-range
0 -> len * elem_size.

### Implementation

Here is our `vec.h` header file with implementation prototypes;

```c

#ifndef VEC_H
# define VEC_H

#include "stdlib.h"
#include "stdint.h"
#include "unistd.h"
#include "string.h"
#include "stdbool.h"

typedef struct s_vec
{
	uint8_t	*memory;
	size_t	elem_size;
	size_t	alloc_size;
	size_t	len;
}	t_vec;

ssize_t	vec_new(t_vec *src, size_t len, size_t elem_size);
void	vec_free(t_vec *src);
ssize_t	vec_from(t_vec *dst, void *src, size_t len, size_t elem_size);
ssize_t vec_push(t_vec *src, void *elem);
ssize_t vec_pop(void *dst, t_vec *src);
ssize_t vec_copy(t_vec *dst, t_vec *src);
void	*vec_get(t_vec *src, size_t index);
ssize_t	vec_insert(t_vec *dst, void *elem, size_t index);
ssize_t	vec_remove(t_vec *src, size_t index);
void	vec_iter(t_vec *src, void (*f) (void *));
void	vec_map(t_vec *dst, t_vec *src, void (*f) (void *));
void	vec_filter(t_vec *dst, t_vec *src, bool (*f) (void *));
void 	vec_reduce(void *dst, t_vec *src, void (*f) (void *, void *));

#endif

```

#### Construction and Destruction

We create a function `vec_new` which will take an allocated pointer `dst` and
allocate len * elem_size amount of bytes in the buffer.

Allocation:

```c

ssize_t vec_new(t_vec *dst, size_t init_alloc, size_t elem_size)
{
	if (!dst || elem_size == 0 || init_alloc == 0)
		return (-1);
	dst->alloc_size = init_alloc * elem_size;
	dst->len = 0;
	dst->elem_size = elem_size;
	dst->memory = malloc(dst->alloc_size);
	if (!dst->memory)
	{
		dst->alloc_size = 0;
		dst->len = 0;
		dst->elem_size = 0;
		return (-1);
	}
	return ((ssize_t)dst->alloc_size);
}

int main(void)
{
	t_vec	v;
	ssize_t	ret;

	ret = vec_new(&v, 10, sizeof(int));
	if (ret < 0)
		printf("Error!");
}

```

Deallocation:

```c

void vec_free(t_vec *src)
{
	if (!src)
		return ;
	free(src->memory);
	src->memory = NULL;
	src->alloc_size = 0;
	src->elem_size = 0;
	src->len = 0;
}

int main(void)
{
	t_vec	v;
	ssize_t	ret;

	ret = vec_new(&v, 10, sizeof(int));
	if (ret < 0)
		printf("Error!");
	vec_free(&v);
}

```

A handy `from` method that creates a vec out of any pointer.

```c

ssize_t vec_from(t_vec *dst, void *src, size_t len, size_t elem_size)
{
	if (!src || !src || elem_size == 0)
		return (-1);
	dst->alloc_size = len * elem_size;
	dst->len = len;
	dst->elem_size = elem_size;
	dst->memory = malloc(dst->alloc_size);
	dst->memory = memcpy(dst->memory, src, dst->alloc_size);
	return ((ssize_t)dst->alloc_size);
}

int main(void)
{
	int		vals[] = {1, 2, 3};
	t_vec	v;
	ssize_t	ret;

	ret = vec_from(&v, vals, 3, sizeof(int));
	if (ret < 0)
		printf("Error!");
	vec_free(&v);
}

```

#### Copying and Resizing

Resizing is used by many functions that we will implement later, so better do it
now. In the resizing function it's handy to call the `vec_copy` method so we'll
implement that as well.

The copy function is very simple and will only copy as many bytes as are
available in the `dst` vector.

```c

ssize_t vec_copy(t_vec *dst, t_vec *src)
{
	size_t	copy_size;

	if (!dst || !src)
		return (-1);
	if (src->len * src->elem_size < dst->alloc_size)
		copy_size = src->len * src->elem_size;
	else
		copy_size = src->alloc_size;
	memcpy(dst->memory, src->memory, copy_size);
	return ((ssize_t)dst->alloc_size);
}

int main(void)
{
	int		vals[] = {1, 2, 3};
	t_vec	v;
	t_vec	d;

	vec_from(&v, vals, 3, sizeof(int));
	vec_new(&v, 3, sizeof(int));
	vec_copy(&d, &v);
	vec_free(&v);
	vec_free(&d);
}

```

The resizing function will take in a `target_size` parameter and either srink
(destructively) or grow the vector to the target size copying the old contents
over to the new alloaction.

```c

static ssize_t vec_resize(t_vec *src, size_t target_size)
{
	t_vec	dst;
	ssize_t	ret;

	if (!src)
		return (-1);
	ret = vec_new(&dst, target_size / src->elem_size, src->elem_size);
	memcpy(dst.memory, src->memory, src->alloc_size);
	dst.len = src->len;
	if (ret < 0)
		return (-1);
	vec_copy(&dst, src);
	vec_free(src);
	*src = dst;
	return ((ssize_t)src->alloc_size);
}

```

#### Manipulating the Elements

We have several functions that we use to manipulate the vector. We can `push`
elements to the end of the array or `pop` them from the end or `get` a pointer
to an element at any index in the vector. These are the least expensive
operations to perform.

```c

ssize_t vec_push(t_vec *dst, void *src)
{
	uint8_t	*target;
	ssize_t	ret;

	if (!dst || !src)
		return (-1);
	if ((dst->elem_size * dst-> len) + 1 > dst-> alloc_size)
	{
		ret = vec_resize(dst, dst->alloc_size * 2);
		if (ret < 0)
			return (-1);
	}
	target = &dst->memory[dst->elem_size * dst->len];
	memcpy(target, src, dst->elem_size);
	dst->len++;
	return ((ssize_t)dst->alloc_size);
}

ssize_t vec_pop(void *dst, t_vec *src)
{
	uint8_t	*target;

	if (!dst || !src)
		return (-1);
	if (src == NULL || src->memory == NULL || src->len == 0)
		return (-1);
	target = &src->memory[src->elem_size * (src->len - 1)];
	memcpy(dst, target, src->elem_size);
	src->len--;
	return ((ssize_t)src->len);
}

void *vec_get(t_vec *src, size_t index)
{
	uint8_t	*ptr;

	if (index > src->len || !src)
		return (NULL);
	ptr = &src->memory[src->elem_size * index];
	return (ptr);
}

int main(void)
{
	int		vals[] = {1, 2, 3};
	int		ret;
	int		*ptr;
	t_vec	v;

	vec_from(&v, vals, 3, sizeof(int));
	vec_push(&v, &vals[0]);
	vec_push(&v, &vals[1]);
	vec_pop(&ret, &v);
	ptr = vec_get(&v, 0);
	vec_free(&v);
}

```

A little more involved operation is inserting or removing an element from any
index in the vector.

```c

ssize_t vec_insert(t_vec *dst, void *src, size_t index)
{
	uint8_t	*pos;
	uint8_t	*mov_pos;
	ssize_t	ret;

	if (!dst || !src || index > dst->len)
		return (-1);
	if (index == dst->len)
		return (vec_push(dst, src));
	if ((dst->elem_size * dst-> len) + 1 > dst-> alloc_size)
	{
		ret = vec_resize(dst, dst->alloc_size * 2);
		if (ret < 0)
			return (-1);
	}
	pos = &dst->memory[dst->elem_size * index];
	mov_pos = &dst->memory[dst->elem_size * (index + 1)];
	memmove(mov_pos, pos, (dst->len - index) * dst->elem_size);
	memcpy(pos, src, dst->elem_size);
	dst->len++;
	return ((ssize_t)dst->alloc_size);
}

ssize_t vec_remove(t_vec *src, size_t index)
{
	uint8_t	*pos;
	uint8_t	*mov_pos;

	if (!src || index > src->len)
		return (-1);
	if (index == src->len)
	{
		src->len--;
		return ((ssize_t)src->alloc_size);
	}
	pos = &src->memory[src->elem_size * (index + 1)];
	mov_pos = &src->memory[src->elem_size * index];
	memmove(mov_pos, pos, (src->len - index) * src->elem_size);
	src->len--;
	return ((ssize_t)src->alloc_size);
}

int main(void)
{
	int		vals[] = {1, 2, 3};
	t_vec	v;

	vec_from(&v, vals, 3, sizeof(int));
	vec_insert(&v, &vals[0], 2);
	vec_remove(&v, 2);
	vec_free(&t1);
}

```

#### Iterator Methods

When we iterate our vector we usually use iterator functions. It's quite
surprising how many use cases these few simple functions cover, reducing errors
and code.

First we create a simple iterator that takes in a function pointer which takes
in a pointer to an element in the loop. These are the actual elements in the
vector so any modification will modify the original values.

```c

void vec_iter(t_vec *src, void (*f) (void *))
{
	void	*ptr;
	size_t	i;

	if (!src)
		return ;
	i = 0;
	while (i < src->len)
	{
		ptr = vec_get(src, i);
		f(ptr);
		i++;
	}
}

void print_int(void *src)
{
	printf("%d\n", *(int *)src);
}

int main(void)
{
	t_vec	t1;
	int		base[] = {1, 2, 3, 4, 5};

	vec_from(&t1, base, 5, sizeof(int);
	vec_iter(&t1, print_int);
	vec_free(&t1);
}

```

Next we make a `map` function which copies over the current element (this operation is
non-destructive), sends the element to `f` and then appends it to a new vector `dst`.


```c

void vec_map(t_vec *dst, t_vec *src, void (*f) (void *))
{
	void	*ptr;
	void	*res;
	size_t	i;

	if (!dst || !src)
		return ;
	res = malloc(dst->elem_size);
	i = 0;
	while (i < src->len)
	{
		ptr = vec_get(src, i);
		memcpy(res, ptr, dst->elem_size);
		f(res);
		vec_push(dst, res);
		i++;
	}
	free(res);
}

```

Example:

```c

void *add_one(void *ptr)
{
	*(int *)src += 1;
}

void main()
{
	t_vec values;

	vec_new(&values, 10, sizeof(int));
	for (int i = 0; i < 10; i++)
		vec_push(&values, &i);
	vec_map(&even, &values, add_one);
}

```

Next a filtering function. Now the function pointer will return `true` or `false`
indicating if the element should be added to the resulting vector.

```c

void vec_filter(t_vec *dst, t_vec *src, bool (*f) (void *))
{
	void	*ptr;
	void	*res;
	size_t	i;

	if (!dst || !src)
		return ;
	res = malloc(dst->elem_size);
	i = 0;
	while (i < src->len)
	{
		ptr = vec_get(src, i);
		memcpy(res, ptr, dst->elem_size);
		if (f(res) == true)
			vec_push(dst, res);
		i++;
	}
	free(res);
}

bool filter_even(void *src)
{
	if (*(int *)src % 2 == 0)
		return (true);
	return (false);
}

void main()
{
	t_vec values;

	vec_new(&values, 10, sizeof(int));
	for (int i = 0; i < 10; i++)
		vec_push(&values, &i);
	vec_filter(&even, &values, filter_even);
}

```

Finally we create a function that iterate's the vector and at each iteration
function `f` get's the `dst` element passed to `vec_reduce` as the first
parameter and the current element as the second parameter allowing reducing the
results of the iteration to one element such as in summing a list of integers.

```c

void vec_reduce(void *dst, t_vec *src, void (*f) (void *, void *))
{
	void	*ptr;
	size_t	i;

	if (!dst || !src)
		return ;
	i = 0;
	while (i < src->len)
	{
		ptr = vec_get(src, i);
		f(dst, ptr);
		i++;
	}
}

void sum(void *dst, void *src)
{
	*(int *)dst += *(int *)src;
}

void main()
{
	t_vec	t1;
	int		base[] = {1, 2, 3, 4, 5};
	int		result = 0;

	vec_from(&t1, base, 5, sizeof(int));
	vec_reduce(&result, &t1, reduce_tester);
	assert(result == 15);
	vec_free(&t1);
}

```

## Conclusions

That's a quick overview on how to implement a performant and easy to use
dynamic data-structure in C. This structure can be used as a fundamental
building block in many programs. The implementation has been kept simple
because frankly it doesn't have to be more complicated. New functions
could definitely be added to grow the functionality of `t_vec` even
further.