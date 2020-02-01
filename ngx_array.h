
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {        // 数组定义
    void        *elts;  // 数组首地址
    ngx_uint_t   nelts; // 第一个未使用元素的索引(类似于栈顶指针)
    size_t       size;  // 每个元素所占空间大小(创建数组时已经固定)
    ngx_uint_t   nalloc;// 分配多少个元素
    ngx_pool_t  *pool;  // 内存池(在内存池上分配)
} ngx_array_t;


ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
void ngx_array_destroy(ngx_array_t *a);
void *ngx_array_push(ngx_array_t *a);
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);

// ngx_int_t 是 nginx 里自定义的类似，这里把它看做是 int 就可以了
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */
    // 上面是官方的注释：先初始化array->nelts，在出初始化array->nelts，否则MSVC认为array->nelts可以在没有初始化的情况下使用
    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#endif /* _NGX_ARRAY_H_INCLUDED_ */
