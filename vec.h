#ifndef VEC_H
# define VEC_H

#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "stdbool.h"

typedef struct s_vec
{
	unsigned char	*memory;
	size_t			elem_size;
	size_t			alloc_size;
	size_t			len;
}	t_vec;

int		vec_new(t_vec *src, size_t init_alloc, size_t elem_size);
void	vec_free(t_vec *src);
int		vec_from(t_vec *dst, void *src, size_t len, size_t elem_size);
int		vec_resize(t_vec *src, size_t target_size);
int	 	vec_push(t_vec *src, void *elem);
int	 	vec_pop(void *dst, t_vec *src);
int	 	vec_copy(t_vec *dst, t_vec *src);
void	*vec_get(t_vec *src, size_t index);
int		vec_insert(t_vec *dst, void *elem, size_t index);
int		vec_remove(t_vec *src, size_t index);
int	 	vec_append(t_vec *dst, t_vec *src);
int	 	vec_prepend(t_vec *dst, t_vec *src);
void	vec_iter(t_vec *src, void (*f) (void *));
int		vec_map(t_vec *dst, t_vec *src, void (*f) (void *));
int		vec_filter(t_vec *dst, t_vec *src, bool (*f) (void *));
int	 	vec_reduce(void *dst, t_vec *src, void (*f) (void *, void *));
void	vec_sort(t_vec *src, int (*f)(void *, void *));

#endif
