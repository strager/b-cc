// Based on OpenBSD's <sys/queue.h> (src/sys/sys/queue.h)
// revision 1.38.

/*	$OpenBSD: queue.h,v 1.38 2013/07/03 15:05:21 fgsch Exp $	*/
/*	$NetBSD: queue.h,v 1.11 1996/05/16 05:17:14 mycroft Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 */

#ifndef B_HEADER_GUARD_B59FA019_C32D_4D35_AD26_E2CD25E35F47
#define B_HEADER_GUARD_B59FA019_C32D_4D35_AD26_E2CD25E35F47

/*
 * This file defines five types of data structures: singly-linked lists, 
 * lists, simple queues, tail queues, and circular queues.
 *
 *
 * A singly-linked list is headed by a single forward pointer. The elements
 * are singly linked for minimum space and pointer manipulation overhead at
 * the expense of O(n) removal for arbitrary elements. New elements can be
 * added to the list after an existing element or at the head of the list.
 * Elements being removed from the head of the list should use the explicit
 * macro for this purpose for optimum efficiency. A singly-linked list may
 * only be traversed in the forward direction.  Singly-linked lists are ideal
 * for applications with large datasets and few or no removals or for
 * implementing a LIFO queue.
 *
 * A list is headed by a single forward pointer (or an array of forward
 * pointers for a hash table header). The elements are doubly linked
 * so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before
 * or after an existing element or at the head of the list. A list
 * may only be traversed in the forward direction.
 *
 * A simple queue is headed by a pair of pointers, one the head of the
 * list and the other to the tail of the list. The elements are singly
 * linked to save space, so elements can only be removed from the
 * head of the list. New elements can be added to the list before or after
 * an existing element, at the head of the list, or at the end of the
 * list. A simple queue may only be traversed in the forward direction.
 *
 * A tail queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or
 * after an existing element, at the head of the list, or at the end of
 * the list. A tail queue may be traversed in either direction.
 *
 * A circle queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or after
 * an existing element, at the head of the list, or at the end of the list.
 * A circle queue may be traversed in either direction, but has a more
 * complex end of list detection.
 *
 * For details on the use of these macros, see the queue(3) manual page.
 */

#define B_Q_INVALIDATE(a) (a) = ((void *)-1)

/*
 * Singly-linked List definitions.
 */
#define B_SLIST_HEAD(name, type) \
struct name { \
	struct type *slh_first;	/* first element */ \
}
 
#define	B_SLIST_HEAD_INITIALIZER(head) \
	{ NULL }
 
#define B_SLIST_ENTRY(type) \
struct { \
	struct type *sle_next;	/* next element */ \
}
 
/*
 * Singly-linked List access methods.
 */
#define	B_SLIST_FIRST(head)	((head)->slh_first)
#define	B_SLIST_END(head)		NULL
#define	B_SLIST_EMPTY(head)	(B_SLIST_FIRST(head) == B_SLIST_END(head))
#define	B_SLIST_NEXT(elm, field)	((elm)->field.sle_next)

#define	B_SLIST_FOREACH(var, head, field) \
	for((var) = B_SLIST_FIRST(head); \
	    (var) != B_SLIST_END(head); \
	    (var) = B_SLIST_NEXT(var, field))

#define	B_SLIST_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_SLIST_FIRST(head); \
	    (var) && ((tvar) = B_SLIST_NEXT(var, field), 1); \
	    (var) = (tvar))

/*
 * Singly-linked List functions.
 */
#define	B_SLIST_INIT(head) { \
	B_SLIST_FIRST(head) = B_SLIST_END(head); \
}

#define	B_SLIST_INSERT_AFTER(slistelm, elm, field) do { \
	(elm)->field.sle_next = (slistelm)->field.sle_next; \
	(slistelm)->field.sle_next = (elm); \
} while (0)

#define	B_SLIST_INSERT_HEAD(head, elm, field) do { \
	(elm)->field.sle_next = (head)->slh_first; \
	(head)->slh_first = (elm); \
} while (0)

#define	B_SLIST_REMOVE_AFTER(elm, field) do { \
	(elm)->field.sle_next = (elm)->field.sle_next->field.sle_next; \
} while (0)

#define	B_SLIST_REMOVE_HEAD(head, field) do { \
	(head)->slh_first = (head)->slh_first->field.sle_next; \
} while (0)

#define B_SLIST_REMOVE(head, elm, type, field) do { \
	if ((head)->slh_first == (elm)) { \
		B_SLIST_REMOVE_HEAD((head), field); \
	} else { \
		struct type *curelm = (head)->slh_first; \
 \
		while (curelm->field.sle_next != (elm)) \
			curelm = curelm->field.sle_next; \
		curelm->field.sle_next = \
		    curelm->field.sle_next->field.sle_next; \
		B_Q_INVALIDATE((elm)->field.sle_next); \
	} \
} while (0)

/*
 * List definitions.
 */
#define B_LIST_HEAD(name, type) \
struct name { \
	struct type *lh_first;	/* first element */ \
}

#define B_LIST_HEAD_INITIALIZER(head) \
	{ NULL }

#define B_LIST_ENTRY(type) \
struct { \
	struct type *le_next;	/* next element */ \
	struct type **le_prev;	/* address of previous next element */ \
}

/*
 * List access methods
 */
#define	B_LIST_FIRST(head)		((head)->lh_first)
#define	B_LIST_END(head)			NULL
#define	B_LIST_EMPTY(head)		(B_LIST_FIRST(head) == B_LIST_END(head))
#define	B_LIST_NEXT(elm, field)		((elm)->field.le_next)

#define B_LIST_FOREACH(var, head, field) \
	for((var) = B_LIST_FIRST(head); \
	    (var)!= B_LIST_END(head); \
	    (var) = B_LIST_NEXT(var, field))

#define	B_LIST_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_LIST_FIRST(head); \
	    (var) && ((tvar) = B_LIST_NEXT(var, field), 1); \
	    (var) = (tvar))

/*
 * List functions.
 */
#define	B_LIST_INIT(head) do { \
	B_LIST_FIRST(head) = B_LIST_END(head); \
} while (0)

#define B_LIST_INSERT_AFTER(listelm, elm, field) do { \
	if (((elm)->field.le_next = (listelm)->field.le_next) != NULL) \
		(listelm)->field.le_next->field.le_prev = \
		    &(elm)->field.le_next; \
	(listelm)->field.le_next = (elm); \
	(elm)->field.le_prev = &(listelm)->field.le_next; \
} while (0)

#define	B_LIST_INSERT_BEFORE(listelm, elm, field) do { \
	(elm)->field.le_prev = (listelm)->field.le_prev; \
	(elm)->field.le_next = (listelm); \
	*(listelm)->field.le_prev = (elm); \
	(listelm)->field.le_prev = &(elm)->field.le_next; \
} while (0)

#define B_LIST_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.le_next = (head)->lh_first) != NULL) \
		(head)->lh_first->field.le_prev = &(elm)->field.le_next; \
	(head)->lh_first = (elm); \
	(elm)->field.le_prev = &(head)->lh_first; \
} while (0)

#define B_LIST_REMOVE(elm, field) do { \
	if ((elm)->field.le_next != NULL) \
		(elm)->field.le_next->field.le_prev = \
		    (elm)->field.le_prev; \
	*(elm)->field.le_prev = (elm)->field.le_next; \
	B_Q_INVALIDATE((elm)->field.le_prev); \
	B_Q_INVALIDATE((elm)->field.le_next); \
} while (0)

#define B_LIST_REPLACE(elm, elm2, field) do { \
	if (((elm2)->field.le_next = (elm)->field.le_next) != NULL) \
		(elm2)->field.le_next->field.le_prev = \
		    &(elm2)->field.le_next; \
	(elm2)->field.le_prev = (elm)->field.le_prev; \
	*(elm2)->field.le_prev = (elm2); \
	B_Q_INVALIDATE((elm)->field.le_prev); \
	B_Q_INVALIDATE((elm)->field.le_next); \
} while (0)

/*
 * Simple queue definitions.
 */
#define B_SIMPLEQ_HEAD(name, type) \
struct name { \
	struct type *sqh_first;	/* first element */ \
	struct type **sqh_last;	/* addr of last next element */ \
}

#define B_SIMPLEQ_HEAD_INITIALIZER(head) \
	{ NULL, &(head).sqh_first }

#define B_SIMPLEQ_ENTRY(type) \
struct { \
	struct type *sqe_next;	/* next element */ \
}

/*
 * Simple queue access methods.
 */
#define	B_SIMPLEQ_FIRST(head)	    ((head)->sqh_first)
#define	B_SIMPLEQ_END(head)	    NULL
#define	B_SIMPLEQ_EMPTY(head)	    (B_SIMPLEQ_FIRST(head) == B_SIMPLEQ_END(head))
#define	B_SIMPLEQ_NEXT(elm, field)    ((elm)->field.sqe_next)

#define B_SIMPLEQ_FOREACH(var, head, field) \
	for((var) = B_SIMPLEQ_FIRST(head); \
	    (var) != B_SIMPLEQ_END(head); \
	    (var) = B_SIMPLEQ_NEXT(var, field))

#define	B_SIMPLEQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_SIMPLEQ_FIRST(head); \
	    (var) && ((tvar) = B_SIMPLEQ_NEXT(var, field), 1); \
	    (var) = (tvar))

/*
 * Simple queue functions.
 */
#define	B_SIMPLEQ_INIT(head) do { \
	(head)->sqh_first = NULL; \
	(head)->sqh_last = &(head)->sqh_first; \
} while (0)

#define B_SIMPLEQ_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.sqe_next = (head)->sqh_first) == NULL) \
		(head)->sqh_last = &(elm)->field.sqe_next; \
	(head)->sqh_first = (elm); \
} while (0)

#define B_SIMPLEQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.sqe_next = NULL; \
	*(head)->sqh_last = (elm); \
	(head)->sqh_last = &(elm)->field.sqe_next; \
} while (0)

#define B_SIMPLEQ_INSERT_AFTER(head, listelm, elm, field) do { \
	if (((elm)->field.sqe_next = (listelm)->field.sqe_next) == NULL) \
		(head)->sqh_last = &(elm)->field.sqe_next; \
	(listelm)->field.sqe_next = (elm); \
} while (0)

#define B_SIMPLEQ_REMOVE_HEAD(head, field) do { \
	if (((head)->sqh_first = (head)->sqh_first->field.sqe_next) == NULL) \
		(head)->sqh_last = &(head)->sqh_first; \
} while (0)

#define B_SIMPLEQ_REMOVE_AFTER(head, elm, field) do { \
	if (((elm)->field.sqe_next = (elm)->field.sqe_next->field.sqe_next) \
	    == NULL) \
		(head)->sqh_last = &(elm)->field.sqe_next; \
} while (0)

/*
 * XOR Simple queue definitions.
 */
#define B_XSIMPLEQ_HEAD(name, type) \
struct name { \
	struct type *sqx_first;	/* first element */ \
	struct type **sqx_last;	/* addr of last next element */ \
	unsigned long sqx_cookie; \
}

#define B_XSIMPLEQ_ENTRY(type) \
struct { \
	struct type *sqx_next;	/* next element */ \
}

/*
 * XOR Simple queue access methods.
 */
#define B_XSIMPLEQ_XOR(head, ptr)	    ((__typeof(ptr))((head)->sqx_cookie ^ \
					(unsigned long)(ptr)))
#define	B_XSIMPLEQ_FIRST(head)	    B_XSIMPLEQ_XOR(head, ((head)->sqx_first))
#define	B_XSIMPLEQ_END(head)	    NULL
#define	B_XSIMPLEQ_EMPTY(head)	    (B_XSIMPLEQ_FIRST(head) == B_XSIMPLEQ_END(head))
#define	B_XSIMPLEQ_NEXT(head, elm, field)    B_XSIMPLEQ_XOR(head, ((elm)->field.sqx_next))


#define B_XSIMPLEQ_FOREACH(var, head, field) \
	for ((var) = B_XSIMPLEQ_FIRST(head); \
	    (var) != B_XSIMPLEQ_END(head); \
	    (var) = B_XSIMPLEQ_NEXT(head, var, field))

#define	B_XSIMPLEQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_XSIMPLEQ_FIRST(head); \
	    (var) && ((tvar) = B_XSIMPLEQ_NEXT(head, var, field), 1); \
	    (var) = (tvar))

/*
 * XOR Simple queue functions.
 */
#define	B_XSIMPLEQ_INIT(head) do { \
	arc4random_buf(&(head)->sqx_cookie, sizeof((head)->sqx_cookie)); \
	(head)->sqx_first = B_XSIMPLEQ_XOR(head, NULL); \
	(head)->sqx_last = B_XSIMPLEQ_XOR(head, &(head)->sqx_first); \
} while (0)

#define B_XSIMPLEQ_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.sqx_next = (head)->sqx_first) == \
	    B_XSIMPLEQ_XOR(head, NULL)) \
		(head)->sqx_last = B_XSIMPLEQ_XOR(head, &(elm)->field.sqx_next); \
	(head)->sqx_first = B_XSIMPLEQ_XOR(head, (elm)); \
} while (0)

#define B_XSIMPLEQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.sqx_next = B_XSIMPLEQ_XOR(head, NULL); \
	*(B_XSIMPLEQ_XOR(head, (head)->sqx_last)) = B_XSIMPLEQ_XOR(head, (elm)); \
	(head)->sqx_last = B_XSIMPLEQ_XOR(head, &(elm)->field.sqx_next); \
} while (0)

#define B_XSIMPLEQ_INSERT_AFTER(head, listelm, elm, field) do { \
	if (((elm)->field.sqx_next = (listelm)->field.sqx_next) == \
	    B_XSIMPLEQ_XOR(head, NULL)) \
		(head)->sqx_last = B_XSIMPLEQ_XOR(head, &(elm)->field.sqx_next); \
	(listelm)->field.sqx_next = B_XSIMPLEQ_XOR(head, (elm)); \
} while (0)

#define B_XSIMPLEQ_REMOVE_HEAD(head, field) do { \
	if (((head)->sqx_first = B_XSIMPLEQ_XOR(head, \
	    (head)->sqx_first)->field.sqx_next) == B_XSIMPLEQ_XOR(head, NULL)) \
		(head)->sqx_last = B_XSIMPLEQ_XOR(head, &(head)->sqx_first); \
} while (0)

#define B_XSIMPLEQ_REMOVE_AFTER(head, elm, field) do { \
	if (((elm)->field.sqx_next = B_XSIMPLEQ_XOR(head, \
	    (elm)->field.sqx_next)->field.sqx_next) \
	    == B_XSIMPLEQ_XOR(head, NULL)) \
		(head)->sqx_last = \
		    B_XSIMPLEQ_XOR(head, &(elm)->field.sqx_next); \
} while (0)

		    
/*
 * Tail queue definitions.
 */
#define B_TAILQ_HEAD(name, type) \
struct name { \
	struct type *tqh_first;	/* first element */ \
	struct type **tqh_last;	/* addr of last next element */ \
}

#define B_TAILQ_HEAD_INITIALIZER(head) \
	{ NULL, &(head).tqh_first }

#define B_TAILQ_ENTRY(type) \
struct { \
	struct type *tqe_next;	/* next element */ \
	struct type **tqe_prev;	/* address of previous next element */ \
}

/* 
 * tail queue access methods 
 */
#define	B_TAILQ_FIRST(head)		((head)->tqh_first)
#define	B_TAILQ_END(head)			NULL
#define	B_TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#define B_TAILQ_LAST(head, headname) \
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
/* XXX */
#define B_TAILQ_PREV(elm, headname, field) \
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#define	B_TAILQ_EMPTY(head) \
	(B_TAILQ_FIRST(head) == B_TAILQ_END(head))

#define B_TAILQ_FOREACH(var, head, field) \
	for((var) = B_TAILQ_FIRST(head); \
	    (var) != B_TAILQ_END(head); \
	    (var) = B_TAILQ_NEXT(var, field))

#define	B_TAILQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_TAILQ_FIRST(head); \
	    (var) != B_TAILQ_END(head) && \
	    ((tvar) = B_TAILQ_NEXT(var, field), 1); \
	    (var) = (tvar))


#define B_TAILQ_FOREACH_REVERSE(var, head, headname, field) \
	for((var) = B_TAILQ_LAST(head, headname); \
	    (var) != B_TAILQ_END(head); \
	    (var) = B_TAILQ_PREV(var, headname, field))

#define	B_TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar) \
	for ((var) = B_TAILQ_LAST(head, headname); \
	    (var) != B_TAILQ_END(head) && \
	    ((tvar) = B_TAILQ_PREV(var, headname, field), 1); \
	    (var) = (tvar))

/*
 * Tail queue functions.
 */
#define	B_TAILQ_INIT(head) do { \
	(head)->tqh_first = NULL; \
	(head)->tqh_last = &(head)->tqh_first; \
} while (0)

#define B_TAILQ_INSERT_HEAD(head, elm, field) do { \
	if (((elm)->field.tqe_next = (head)->tqh_first) != NULL) \
		(head)->tqh_first->field.tqe_prev = \
		    &(elm)->field.tqe_next; \
	else \
		(head)->tqh_last = &(elm)->field.tqe_next; \
	(head)->tqh_first = (elm); \
	(elm)->field.tqe_prev = &(head)->tqh_first; \
} while (0)

#define B_TAILQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.tqe_next = NULL; \
	(elm)->field.tqe_prev = (head)->tqh_last; \
	*(head)->tqh_last = (elm); \
	(head)->tqh_last = &(elm)->field.tqe_next; \
} while (0)

#define B_TAILQ_INSERT_AFTER(head, listelm, elm, field) do { \
	if (((elm)->field.tqe_next = (listelm)->field.tqe_next) != NULL) \
		(elm)->field.tqe_next->field.tqe_prev = \
		    &(elm)->field.tqe_next; \
	else \
		(head)->tqh_last = &(elm)->field.tqe_next; \
	(listelm)->field.tqe_next = (elm); \
	(elm)->field.tqe_prev = &(listelm)->field.tqe_next; \
} while (0)

#define	B_TAILQ_INSERT_BEFORE(listelm, elm, field) do { \
	(elm)->field.tqe_prev = (listelm)->field.tqe_prev; \
	(elm)->field.tqe_next = (listelm); \
	*(listelm)->field.tqe_prev = (elm); \
	(listelm)->field.tqe_prev = &(elm)->field.tqe_next; \
} while (0)

#define B_TAILQ_REMOVE(head, elm, field) do { \
	if (((elm)->field.tqe_next) != NULL) \
		(elm)->field.tqe_next->field.tqe_prev = \
		    (elm)->field.tqe_prev; \
	else \
		(head)->tqh_last = (elm)->field.tqe_prev; \
	*(elm)->field.tqe_prev = (elm)->field.tqe_next; \
	B_Q_INVALIDATE((elm)->field.tqe_prev); \
	B_Q_INVALIDATE((elm)->field.tqe_next); \
} while (0)

#define B_TAILQ_REPLACE(head, elm, elm2, field) do { \
	if (((elm2)->field.tqe_next = (elm)->field.tqe_next) != NULL) \
		(elm2)->field.tqe_next->field.tqe_prev = \
		    &(elm2)->field.tqe_next; \
	else \
		(head)->tqh_last = &(elm2)->field.tqe_next; \
	(elm2)->field.tqe_prev = (elm)->field.tqe_prev; \
	*(elm2)->field.tqe_prev = (elm2); \
	B_Q_INVALIDATE((elm)->field.tqe_prev); \
	B_Q_INVALIDATE((elm)->field.tqe_next); \
} while (0)

/*
 * Circular queue definitions.
 */
#define B_CIRCLEQ_HEAD(name, type) \
struct name { \
	struct type *cqh_first;		/* first element */ \
	struct type *cqh_last;		/* last element */ \
}

#define B_CIRCLEQ_HEAD_INITIALIZER(head) \
	{ B_CIRCLEQ_END(&head), B_CIRCLEQ_END(&head) }

#define B_CIRCLEQ_ENTRY(type) \
struct { \
	struct type *cqe_next;		/* next element */ \
	struct type *cqe_prev;		/* previous element */ \
}

/*
 * Circular queue access methods 
 */
#define	B_CIRCLEQ_FIRST(head)		((head)->cqh_first)
#define	B_CIRCLEQ_LAST(head)		((head)->cqh_last)
#define	B_CIRCLEQ_END(head)		((void *)(head))
#define	B_CIRCLEQ_NEXT(elm, field)	((elm)->field.cqe_next)
#define	B_CIRCLEQ_PREV(elm, field)	((elm)->field.cqe_prev)
#define	B_CIRCLEQ_EMPTY(head) \
	(B_CIRCLEQ_FIRST(head) == B_CIRCLEQ_END(head))

#define B_CIRCLEQ_FOREACH(var, head, field) \
	for((var) = B_CIRCLEQ_FIRST(head); \
	    (var) != B_CIRCLEQ_END(head); \
	    (var) = B_CIRCLEQ_NEXT(var, field))

#define	B_CIRCLEQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = B_CIRCLEQ_FIRST(head); \
	    (var) != B_CIRCLEQ_END(head) && \
	    ((tvar) = B_CIRCLEQ_NEXT(var, field), 1); \
	    (var) = (tvar))

#define B_CIRCLEQ_FOREACH_REVERSE(var, head, field) \
	for((var) = B_CIRCLEQ_LAST(head); \
	    (var) != B_CIRCLEQ_END(head); \
	    (var) = B_CIRCLEQ_PREV(var, field))

#define	B_CIRCLEQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar) \
	for ((var) = B_CIRCLEQ_LAST(head, headname); \
	    (var) != B_CIRCLEQ_END(head) && \
	    ((tvar) = B_CIRCLEQ_PREV(var, headname, field), 1); \
	    (var) = (tvar))

/*
 * Circular queue functions.
 */
#define	B_CIRCLEQ_INIT(head) do { \
	(head)->cqh_first = B_CIRCLEQ_END(head); \
	(head)->cqh_last = B_CIRCLEQ_END(head); \
} while (0)

#define B_CIRCLEQ_INSERT_AFTER(head, listelm, elm, field) do { \
	(elm)->field.cqe_next = (listelm)->field.cqe_next; \
	(elm)->field.cqe_prev = (listelm); \
	if ((listelm)->field.cqe_next == B_CIRCLEQ_END(head)) \
		(head)->cqh_last = (elm); \
	else \
		(listelm)->field.cqe_next->field.cqe_prev = (elm); \
	(listelm)->field.cqe_next = (elm); \
} while (0)

#define B_CIRCLEQ_INSERT_BEFORE(head, listelm, elm, field) do { \
	(elm)->field.cqe_next = (listelm); \
	(elm)->field.cqe_prev = (listelm)->field.cqe_prev; \
	if ((listelm)->field.cqe_prev == B_CIRCLEQ_END(head)) \
		(head)->cqh_first = (elm); \
	else \
		(listelm)->field.cqe_prev->field.cqe_next = (elm); \
	(listelm)->field.cqe_prev = (elm); \
} while (0)

#define B_CIRCLEQ_INSERT_HEAD(head, elm, field) do { \
	(elm)->field.cqe_next = (head)->cqh_first; \
	(elm)->field.cqe_prev = B_CIRCLEQ_END(head); \
	if ((head)->cqh_last == B_CIRCLEQ_END(head)) \
		(head)->cqh_last = (elm); \
	else \
		(head)->cqh_first->field.cqe_prev = (elm); \
	(head)->cqh_first = (elm); \
} while (0)

#define B_CIRCLEQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.cqe_next = B_CIRCLEQ_END(head); \
	(elm)->field.cqe_prev = (head)->cqh_last; \
	if ((head)->cqh_first == B_CIRCLEQ_END(head)) \
		(head)->cqh_first = (elm); \
	else \
		(head)->cqh_last->field.cqe_next = (elm); \
	(head)->cqh_last = (elm); \
} while (0)

#define	B_CIRCLEQ_REMOVE(head, elm, field) do { \
	if ((elm)->field.cqe_next == B_CIRCLEQ_END(head)) \
		(head)->cqh_last = (elm)->field.cqe_prev; \
	else \
		(elm)->field.cqe_next->field.cqe_prev = \
		    (elm)->field.cqe_prev; \
	if ((elm)->field.cqe_prev == B_CIRCLEQ_END(head)) \
		(head)->cqh_first = (elm)->field.cqe_next; \
	else \
		(elm)->field.cqe_prev->field.cqe_next = \
		    (elm)->field.cqe_next; \
	B_Q_INVALIDATE((elm)->field.cqe_prev); \
	B_Q_INVALIDATE((elm)->field.cqe_next); \
} while (0)

#define B_CIRCLEQ_REPLACE(head, elm, elm2, field) do { \
	if (((elm2)->field.cqe_next = (elm)->field.cqe_next) == \
	    B_CIRCLEQ_END(head)) \
		(head)->cqh_last = (elm2); \
	else \
		(elm2)->field.cqe_next->field.cqe_prev = (elm2); \
	if (((elm2)->field.cqe_prev = (elm)->field.cqe_prev) == \
	    B_CIRCLEQ_END(head)) \
		(head)->cqh_first = (elm2); \
	else \
		(elm2)->field.cqe_prev->field.cqe_next = (elm2); \
	B_Q_INVALIDATE((elm)->field.cqe_prev); \
	B_Q_INVALIDATE((elm)->field.cqe_next); \
} while (0)

#endif
