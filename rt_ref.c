/* the zion garbage collector */
#include <assert.h>
#include <signal.h>
#include "zion_rt.h"

struct type_info_t {
	/* the id for the type - a unique number */
	type_id_t type_id;

	/* refs_count gives the type-map for memory management/ref counting. */
	int16_t refs_count;

	/* ref_offsets is the list of offsets to managed members */
	int16_t *ref_offsets;

	/* a helpful name for this type */
	const char *name;

	/* the size of the allocation for memory profiling purposes */
	int64_t size;
};

struct var_t {
	/* each runtime variable has a pointer to its type info */
	struct type_info_t *type_info;

	/* and a ref-count of its own */
	zion_int_t ref_count;

#ifdef ZION_DETECT_MEMLEAKS
	int64_t allocation;
	struct var_t *next;
	struct var_t *prev;
#endif

	//////////////////////////////////////
	// THE ACTUAL DATA IS APPENDED HERE //
	//////////////////////////////////////
};

struct tag_t {
	struct type_info_t *type_info;

	/* tags don't have refcounts - as described in their refs_count of -1 */
};

/* An example tag (for use in examining LLIR) */
struct type_info_t __tag_type_info_Example = {
	.type_id = 42,
	.name = "True",
	.refs_count = 0,
	.ref_offsets = 0,
	.size = 0,
};

struct tag_t __tag_Example = {
	.type_info = &__tag_type_info_Example,
};

struct var_t *Example = (struct var_t *)&__tag_Example;


static size_t _bytes_allocated = 0;
static size_t _all_bytes_allocated = 0;

void *mem_alloc(zion_int_t cb) {
	_bytes_allocated += cb;
	_all_bytes_allocated += cb;
	printf("memory allocation is at %ld %ld\n", _bytes_allocated,
			_all_bytes_allocated);

	return calloc(cb, 1);
}

void mem_free(void *p, size_t cb) {
	_bytes_allocated -= cb;
	free(p);
	printf("memory allocation is at %ld %ld\n", _bytes_allocated,
			_all_bytes_allocated);
}

const char *_zion_rt = "zion-rt: ";

#define MEM_PANIC(msg, str, error_code) \
	do { \
		write(2, _zion_rt, strlen(_zion_rt)); \
		write(2, msg, strlen(msg)); \
		write(2, str, strlen(str)); \
		write(2, "\n", 1); \
		exit(error_code); \
	} while (0)


#ifdef ZION_DETECT_MEMLEAKS
struct var_t head_var = {
	.type_info = &__tag_type_info_Example,
	.ref_count = 1,
	.allocation = 0,
	.next = 0,
	.prev = 0,
};
#endif

void check_node_existence(struct var_t *node, zion_bool_t should_exist) {
	struct var_t *p = &head_var;
	assert(p->prev == 0);

	if (should_exist) {
		assert(p->next != 0);
		assert(node != 0);
		assert(node->prev != (struct var_t *)0xdeadbeef && node->next != (struct var_t *)0xdeadbeef);
		assert(node->prev != 0);
	}

	while (p != 0) {
		if (p == node) {
			if (!should_exist) {
				printf("node 0x%08lx of type %s already exists!\n",
						(intptr_t)node, node->type_info->name);
				assert(should_exist);
			} else {
				/* found it, and that's expected. */
				return;
			}
		}
		p = p->next;
	}

	if (should_exist) {
		printf("node 0x%08lx #%ld of type %s does not exist in memory tracking list!\n",
				(intptr_t)node, node->allocation, node->type_info->name);
		assert(!should_exist);
	}
}

void addref_var(struct var_t *var, const char *reason) {
	printf("attempt to addref 0x08%lx because \"%s\"\n", (intptr_t)var, reason);
	if (var == 0) {
		return;
	} else if (var->type_info == 0) {
		MEM_PANIC("attempt to addref a value with a null type_info", "", 111);
	} else if (var->type_info->refs_count >= 0) {
		check_node_existence(var, 1 /* should_exist */);

		++var->ref_count;
#ifdef ZION_DETECT_MEMLEAKS
		printf("addref %s #%ld 0x%08lx to (%ld)\n",
				var->type_info->name,
				var->allocation, (intptr_t)var, var->ref_count);
#else
		printf("addref %s 0x%08lx to (%ld)\n",
				var->type_info->name,
				(intptr_t)var, var->ref_count);
#endif
	} else {
		printf("attempt to addref a singleton of type %s\n", var->type_info->name);
	}
}

#ifdef ZION_DETECT_MEMLEAKS
void add_node(struct var_t *node) {
	assert(node->ref_count == 1);

	check_node_existence(node, 0 /* should_exist */);

	if (node->prev != 0 || node->next != 0) {
		printf("node 0x%08lx #%ld of type %s already has prev and next ptrs?!\n",
				(intptr_t)node, node->allocation, node->type_info->name);
		exit(-1);
	}

	assert(!head_var.next || head_var.next->prev == &head_var);

	node->prev = &head_var;
	node->next = head_var.next;
	if (node->next != 0) {
		node->next->prev = node;
	}
	head_var.next = node;

	assert(head_var.prev == 0);
	assert(head_var.next->prev == &head_var);
	assert(node->prev->next == node);
	assert(!node->next || node->next->prev == node);

	check_node_existence(node, 1 /* should_exist */);
}

void remove_node(struct var_t *node) {
	assert(node->ref_count == 0);

	check_node_existence(node, 1 /* should_exist */);

	assert(node->prev->next == node);
	assert(!node->next || node->next->prev == node);

	node->prev->next = node->next;
	if (node->next != 0) {
		node->next->prev = node->prev;
	}
	node->next = (struct var_t *)0xdeadbeef;
	node->prev = (struct var_t *)0xdeadbeef;

	check_node_existence(node, 0 /* should_exist */);
}

#endif // ZION_DETECT_MEMLEAKS

void release_var(struct var_t *var, const char *reason) {
	if (var == 0) {
		return;
	}

	assert(var->type_info != 0);

	printf("attempt to release var 0x%08lx because \"%s\"\n",
			(intptr_t)var,
			reason);

	if (var->type_info->refs_count >= 0) {
		check_node_existence(var, 1 /* should_exist */);

		assert(var->ref_count > 0);

		// TODO: atomicize for multi-threaded purposes
		--var->ref_count;

#ifdef ZION_DETECT_MEMLEAKS
		printf("release %s #%ld 0x%08lx to (%ld)\n",
				var->type_info->name, var->allocation, (intptr_t)var,
				var->ref_count);
#else
		printf("release %s 0x%08lx to (%ld)\n",
				var->type_info->name, (intptr_t)var,
				var->ref_count);
#endif

		if (var->ref_count == 0) {
			for (int16_t i = var->type_info->refs_count - 1; i >= 0; --i) {
				struct var_t *ref = *(struct var_t **)(((char *)var) + var->type_info->ref_offsets[i]);
				printf("recursively calling release_var on offset %ld of %s which is 0x%08lx\n",
						(intptr_t)var->type_info->ref_offsets[i],
						var->type_info->name,
						(intptr_t)ref);
				release_var(ref, "release recursion");
			}

#ifdef ZION_DETECT_MEMLEAKS
			printf("freeing %s #%ld 0x%08lx\n",
					var->type_info->name,
					var->allocation,
					(intptr_t)var);
			remove_node(var);
#else
			printf("freeing %s 0x%08lx\n",
					var->type_info->name,
					(intptr_t)var);
#endif

			mem_free(var, var->type_info->size);
		}
	} else {
		printf("attempt to release a singleton of type %s\n", var->type_info->name);
	}
}

zion_bool_t isnil(struct var_t *p) {
	return p == 0;
}

type_id_t get_var_type_id(struct var_t *var) {
	if (var != 0) {
		return var->type_info->type_id;
	} else {
		MEM_PANIC("attempt to get_var_type_id of a null value ", "", 116);
		return 0;
	}
}

int64_t _allocation = 1;

struct var_t *create_var(struct type_info_t *type_info)
{
	/* allocate the variable tracking object */
	struct var_t *var = (struct var_t *)mem_alloc(type_info->size);
	var->type_info = type_info;
	var->ref_count = 1;
	var->allocation = _allocation;
	_allocation += 1;

#ifdef ZION_DETECT_MEMLEAKS
	add_node(var);
#endif

	printf("creating %s #%ld 0x%08lx\n", type_info->name, var->allocation, (intptr_t)var);
	return var;
}
