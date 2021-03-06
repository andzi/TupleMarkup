/*
 * Copyright (C) 2012 John Judnich
 * Released as open-source under The MIT Licence.
 *
 * This parser loads an entire TML file into memory very efficiently in both time and space.
 * 
 * Storage space overhead is very low, with only 1 extra byte per leaf node and exactly 10 bytes
 * for nonleaf (list) nodes. Malloc is called only once, and realloc is rarely used.
 *
 * The parsing process consists of reading from the token stream and writing variable length
 * node data (along with leaf node contents strings) into one big array buffer. All node data is
 * contained within this buffer, so malloc isn't used except to initially create this buffer.
 */

#pragma once
#ifndef _TML_PARSER_H__
#define _TML_PARSER_H__

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _MSC_VER
#define TML_INLINE __inline
#else
#define TML_INLINE __inline__
#endif


/* We use this typedef because 32 bit offset values are fine for any TML file under 4 GB.
 * If you need to load TML files over 4 GB into memory, then this can be changed larger. */
typedef uint32_t tml_offset_t; 
/* This should be kept consistent to be pow(2, sizeof(tml_offset_t))-1 */
#define TML_PARSER_MAX_DATA_SIZE 0xFFFFFFFF


struct tml_node
{
	/* If this is a leaf "word" node, this string contains the contents of that word.
	 * If on the other hand this is a list node, this will be an empty string "".
	 * (It is guaranteed to never be NULL, even for TML_NODE_NULL nodes) */
	const char *value;

	/* This will be 0 if there is no next sibling. If nonzero, do not try to use the value yourself.
	 * Use the tml_next_sibling() function to return a new struct tml_node corresponding to the sibling.*/
	tml_offset_t next_sibling;

	/* This will be 0 if this has no child nodes. If nonzero, do not try to use the value yourself.
	 * Use the tml_first_child() function to return a new struct tml_node corresponding to the child.*/
	tml_offset_t first_child;

	/* This will be 0 if this is a "null" node. If nonzero, do NOT try to use or change the value yourself. */
	char *buff;
};

struct tml_doc
{
	/* This contains the root node for data represented by the tml_doc object */
	struct tml_node root_node;

	/* This contains an error description string when a parse error occurred, or NULL if no errors */
	const char *error_message;

	/* INTERNAL - Do not touch. This is the internal data buffer where all node data and strings are stored */
	char *buff;
	size_t buff_index, buff_allocated;
};

/* Iteration functions return this tml_node value when there's no such next node to return */
extern const struct tml_node TML_NODE_NULL;


/* --------------- DATA PARSE FUNCTIONS -------------------- */

/* Create a new tml_doc object, parsing from TML text contained within the given C string. */
struct tml_doc *tml_parse_string(const char *str);

/* Create a new tml_doc object, parsing from TML text from the file specified (by filename). */
struct tml_doc *tml_parse_file(const char *filename);

/* Create a new tml_doc object, parsing from TML text contained within the given memory buffer.
 * The parsing procedure allocates its own memory for parsed data, so you can delete your "buff"
 * data right after calling this if you want. */
struct tml_doc *tml_parse_memory(const char *buff, size_t buff_size);

/* Create a new tml_doc object, parsing from TML text contained within the given memory buffer,
 * using the given memory buffer as a parser working space to conserve memory (less malloc's). This 
 * means that your "buff" data may be modified by the parsing process, so consider the data invalidated 
 * after calling this. The parsing procedure creates its own internal memory for parsed data, so you can
 * delete the "buff" data right after calling this. */
struct tml_doc *tml_parse_in_memory(char *buff, size_t buff_size);

/* Call this to destroy a tml_doc object (do NOT just use free() on a tml_doc* object) 
 * Warning: Once you call this, all tml_node values derived from this data object will be invalidated. */
void tml_free_doc(struct tml_doc *data);


/* --------------- NODE ITERATION FUNCTIONS -------------------- */

/* Returns a new tml_node representing the next sibling after this node, if it exists.
 * If no such sibling exists, a null tml_node value will be returned. Test for this
 * null output condition with tml_is_node_null() */
struct tml_node tml_next_sibling(const struct tml_node *node); /* O(1) time */

/* Returns a new tml_node representing the first child under this node, if it exists. 
 * If no such child exists, a null tml_node value will be returned. Test for this
 * null output condition with tml_is_node_null() */
struct tml_node tml_first_child(const struct tml_node *node); /* O(1) time */

/* Returns true if this is the null node (TML_NODE_NULL). A null node is a special
 * node that doesn't actually exist anywhere within your TML file. It's used to 
 * indicate the end of an iteration. Specifically, when tml_next_sibling() or 
 * tml_first_child() are called but no appropriate successor exists, TML_NODE_NULL
 * is returned. Use this function quickly check if this node is TML_NODE_NULL. */
static TML_INLINE bool tml_is_null(const struct tml_node *node)
{
	return node->buff == 0;
}

/* Returns true if this node contains one or more children.
 * Equivalent to !tml_is_null(tml_first_child(node)), but slightly faster.
 * NOTE: An empty list "[]" is an exceptional case you should watch out for,
 * where it's technically a list but has no children. So a node having no children
 * doesn't necessarily mean it's not a list. If you want to determine if something 
 * is a list, use tml_is_list() instead. */
static TML_INLINE bool tml_has_children(const struct tml_node *node)
{
	return node->first_child != 0;
}

/* Returns true if this node is a list of zero or more subnodes.
 * Equivalent to (strcmp(node->value,"")==0), because lists have no string value.
 * NOTE: A list node doesn't guarantee it will have a child. It's possible that a node
 * is a list node without children (TML allows empty list expressions "[]"). */
static TML_INLINE bool tml_is_list(const struct tml_node *node)
{
	return node->value[0] == '\0';
}

/* Returns the number of children this node contains
 * WARNING: This runs in O(n) time where n is the number of child nodes. */
int tml_child_count(const struct tml_node *node);

/* Returns the nth child of this node indexed by child_index (base 0).
 * WARNING: This runs in O(child_index) linear time.*/
struct tml_node tml_child_at_index(const struct tml_node *node, int child_index);


/* --------------- UTILITY FUNCTIONS (CONVERSION) -------------------- */

/* Converts the contents of this node into a string, with TML syntax stripped out. For example
 * if the node represents the subtree "[a [b [c]] d]", the result will be "a b c d".
 * Returns the length of the resulting string. */
size_t tml_node_to_string(const struct tml_node *node, char *dest_str, size_t dest_str_size);

/* Converts the contents of this node into a string, with auto-formatted TML syntax included.
 * For example if the node represents the subtree "[a [b [c]] d]", the result will be "[a [b [c]] d]".
 * Returns the length of the resulting string. */
size_t tml_node_to_markup_string(const struct tml_node *node, char *dest_str, size_t dest_str_size);

/* Converts the value of this node into a float value. */
float tml_node_to_float(const struct tml_node *node);

/* Converts the value of this node into an double value */
double tml_node_to_double(const struct tml_node *node);

/* Converts the value of this node into an integer value */
int tml_node_to_int(const struct tml_node *node);

/* Reads a list of float values (e.g. "[0.2 1.5 0.8]") into the given float array,
 * up to a maximum of array_size items. Returns the number of values read. */
int tml_node_to_float_array(const struct tml_node *node, float *array, int array_size);

/* Reads a list of double values (e.g. "[0.2 1.5 0.8]") into the given double array,
 * up to a maximum of array_size items. Returns the number of values read. */
int tml_node_to_double_array(const struct tml_node *node, double *array, int array_size);

/* Reads a list of int values (e.g. "[1 2 3]") into the given int array,
 * up to a maximum of array_size items. Returns the number of values read. */
int tml_node_to_int_array(const struct tml_node *node, int *array, int array_size);


/* --------------- UTILITY FUNCTIONS (COMPARISON / PATTERN MATCHING AND SEARCH) -------------------- */

/* tml_compare_nodes: This is a powerful function which allows you not only to compare two nodes for 
 * value equality, but also match a node against a pattern with wildcards. 
 *
 * For standard equality comparison, this works very straightforwardly. It will return true if both 
 * sides are equivalent (e.g. would return true for left = [1 2 3] and the right = [1 2 3], even if
 * they're from different nodes from the tree or entirely different tml_doc objects).
 *
 * Pattern matching allows you to match the left side against a pattern on the right side which may
 * include wildcards. There are two types of wildcards, written as "\*" and "\?" in TML.
 * 
 * The \? wildcard represents any single node - either a list item or a leaf item.
 * The \* wildcard represents zero or more nodes, i.e. it matches anything up to the end of a list.
 * 
 * Simple examples:
 *
 * [a b c] matches [\? \? \?]. It also matches [\*], [\? \*], [\? \? \*], and [\? \? \? \*].
 * [a b c] matches [\? b c]. [a b c] matches [a \? \?]. [a b c] does not match [\? hello hi].
 * [a b c] does not match [\? \? \? \?] for example, because the pattern expects four nodes.
 * [a b c] also does not match [\? \?] because the pattern expects exactly two nodes.
 * The pattern [\? \*] expects one or more nodes, which accepts [a], [a b], [a b c], etc.
 * Pattern [\*] expects zero or more nodes. So this would match [], [a], [a b], [a b c], etc.
 *
 * Limitation: For simplicity (less ambiguity), pattern lists aren't allowed to have anything
 * after a \* wildcard. In other words, patterns like "[a b \*]" are valid but "[\* a b]" are
 * not. This limitation does not apply to "\?": patterns like "[\? a b]" are perfectly fine.
 *
 * Nested list patterns:
 * 
 * Keep in mind that each matched node can be lists too, so for example:
 * [\?] matches [ [a b c] ], because the \? wildcard matches the single nested list.
 *
 * Your patterns can be nested as deeply as you like, and can contain non-wildcard items of 
 * course to match against. For example, say we want to match TML code like
 * "[[bold] [hello this is a test]]" (or equivalently "[bold | hello this is a test]")
 * in such a way that the right nested list can contain anything at all. So the same
 * pattern should also match "[bold | pattern matching is fun]". The following pattern
 * achieves this: "[bold | \*]". Pretty simple and easy once you understand how it works.
 * In this case "[bold | \*]" (which could also be written "[[bold] [\*]]" with absolutely
 * no functional difference) matches any nodes that contain a nested "[bold]" on the left
 * and a nested list of anything on the right.
 *
 * More examples:
 *
 * [bold | hi how are you?] matches [bold | \*]
 * [position | \? \? \?] matches [position | 2.9 3.7 1.0] and [position | 1 2 3]
 * [ vertices | [1 2 3] [5 6 7] [7 8 9] ] matches [vertices | \*]
 *
 * (etc.)
 */
bool tml_compare_nodes(const struct tml_node *candidate, const struct tml_node *pattern);

/* Finds the first child under this node that matches the given pattern, if any.
 * For example searching [1 2 [a 3] 4 [a 5] 6] for the pattern [a \?] will return a node
 * pointing to the [a 3] node, because it's the first child that matches [a \?]. Then,
 * if you want to iterate to the next one "[a 5]", use tml_find_next_sibling.
 * See tml_compare_nodes() for more info on how pattern matching works. */
struct tml_node tml_find_first_child(const struct tml_node *node, const struct tml_node *pattern);

/* Finds the next sibling after this node that matches the given pattern, if any.
 * See tml_compare_nodes() for more info on how pattern matching works. */
struct tml_node tml_find_next_sibling(const struct tml_node *node, const struct tml_node *pattern);



#endif
