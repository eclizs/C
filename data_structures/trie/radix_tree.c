#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUMCHAR 128
#define MAX_BUFFER_SIZE 1000

typedef struct Node
{
   struct Node* child[NUMCHAR];
   char* prefix;
   bool isEndOfWord;
} Node;

typedef struct RadixTree
{
   Node* root;
} RadixTree;

/**
 * Initialize a node with a prefix
 */
Node* createNode(const char* prefix)
{
   Node* node = calloc(1, sizeof(*node));

   // Exit the program if calloc fails
   if (!node)
   {
      perror("calloc");
      exit(EXIT_FAILURE);
   }

   node->prefix = strdup(prefix);

   return node;
}

/**
 * Helper function to determine the length of common characters between a and b
 */
int commonPrefixLength(const char* a, const char* b)
{
   int length = 0;
   int lenA = strlen(a);
   int lenB = strlen(b);
   while (length < lenA && length < lenB && a[length] == b[length]) length++;

   return length;
}

/**
 * Return true if every byte in str can be used as an index into child.
 */
bool hasValidCharacters(const char* str)
{
   if (str == NULL) return false;

   const unsigned char* character = (const unsigned char*)str;
   while (*character != '\0')
   {
      if (*character >= NUMCHAR) return false;
      character++;
   }

   return true;
}

/**
 * Insert a word into the tree.
 * Uses a double pointer since the root may be modified.
 * Basically the same as returning a Node pointer and assigning radix_insert to
 * root everytime it is called root = radix_insert(root, word) as opposed to
 * radix_insert(&root, word).
 */
void radix_insert(Node** root, const char* word)
{
   if (root == NULL || !hasValidCharacters(word)) return;
   // Create a node if root is null
   if (*root == NULL) *root = createNode("");

   Node* node = *root;
   int i = 0;

   int wordLength = strlen(word);

   while (i < wordLength)
   {
      unsigned char key = (unsigned char)word[i];
      int childExists = node->child[key] == NULL ? 0 : 1;

      // If the node doesn't have a child at that key, create a new node
      if (!childExists)
      {
         Node* newNode = createNode(word + i);  // prefix starts at the i-th index of the word
         newNode->isEndOfWord = true;
         node->child[key] =
             newNode; /** Make the new node the child of the original node at index key */
         break;
      }

      node = node->child[key];  // Traverse down the tree

      int commonLength = commonPrefixLength(word + i, node->prefix);
      i += commonLength; /** i jumps over to the first character that doesn't match node->prefix */

      if (i == wordLength)
         node->isEndOfWord = true; /** Current node becomes an end of a word if the entire word is
                                      contained within node->prefix */

      if (commonLength < strlen(node->prefix))
      {
         Node* newChild =
             createNode(node->prefix +
                        commonLength); /** Prefix starts at index commonLength of node->prefix */
         newChild->isEndOfWord = node->isEndOfWord;
         /**
          * Child node follows the original node's isEndOfWord value since
          * the only change done was branching off into a new leaf
          */

         for (int j = 0; j < NUMCHAR; j++)
         {
            newChild->child[j] = node->child[j];  // Copy every child of node into the new child
            node->child[j] = NULL;
         }

         char* temp = node->prefix;
         node->prefix = strndup(node->prefix, commonLength); /** Slice the prefix from
                                                                 index 0 until commonLength */

         free(temp);                                  /** Free the old prefix */
         unsigned char childKey = (unsigned char)newChild->prefix[0];
         node->child[childKey] = newChild; /** new child becomes the child of node */
         node->isEndOfWord =
             (i == wordLength); /** if i == wordLength then i is at the end of the word */
      }
   }
}

/**
 * Helper function for print.
 * builds an initial prefix buffer to be at the start of every word printed by
 * print. parameter size is the size of the buffer.
 */
Node* getNode(Node* root, const char* prefix, char* buffer)
{
   if (root == NULL || buffer == NULL || !hasValidCharacters(prefix)) return NULL;

   Node* node = root;

   int i = 0;
   int prefixLength = strlen(prefix);

   while (i < prefixLength)
   {
      // Get the child that shares the same character as the first character of the remaining prefix
      unsigned char key = (unsigned char)prefix[i];
      Node* child = node->child[key];

      if (!child) return NULL;

      int childPrefixLength = strlen(child->prefix);
      if (childPrefixLength >= MAX_BUFFER_SIZE - i)
         return NULL;  // Prevent buffer overflow by exiting early

      // Append the child's prefix to the buffer
      memcpy(buffer + i, child->prefix, childPrefixLength);
      buffer[i + childPrefixLength] = '\0';

      int commonLength = commonPrefixLength(prefix + i, child->prefix);
      int remainingLength = prefixLength - i;

      if (commonLength == remainingLength)
         return child;  // Child node already represents the entire prefix so return the child
      if (commonLength != childPrefixLength)
         return NULL;  // Otherwise the prefix doesn't match anything so return NULL

      node = child;       // Traverse down to the child node
      i += commonLength;  // i jumps over to the first character that doesn't match child->prefix
   }

   return node;
}

/**
 * Recursive print function.
 * length is the length of the current buffer
 */
void radix_print_rec(Node* node, char* buffer, int length)
{
   if (node == NULL || length >= MAX_BUFFER_SIZE) return;
   if (node->isEndOfWord)
   {
      buffer[length] = '\0';  // Null-terminate the string
      printf("%s\n", buffer);
   }

   for (int i = 0; i < NUMCHAR; i++)
   {
      if (node->child[i] != NULL)
      {
         int prefixLength = strlen(node->child[i]->prefix);
         memcpy(buffer + length, node->child[i]->prefix,
                prefixLength);  // Copy the child's prefix into buffer at index length
         radix_print_rec(node->child[i], buffer,
                         length + prefixLength);  // Recursively go down the tree
      }
   }
}

/**
 * Wrapper function for printing out words starting with prefix
 */
void radix_print(Node* root, const char* prefix)
{
   char buffer[MAX_BUFFER_SIZE] = {0};  // Initialize buffer to '\0'

   Node* node = getNode(root, prefix, buffer); /** Finds the node that shares its prefix with the
                                                  prefix provided by the function */

   if (node == NULL)
   {
      printf("No matches found\n");
      return;
   }

   int prefixLength = strlen(buffer);

   radix_print_rec(node, buffer, prefixLength);
}

/* Helper function for radix_delete.
 * Determine if a node is a leaf or a parent (has children).
 */
bool nodeHasChildren(Node* node)
{
   if (node == NULL) return false;

   for (int i = 0; i < NUMCHAR; i++)
   {
      if (node->child[i] != NULL) return true;  // Node has a child, so return true
   }
   return false;
}

/**
 * Recursive delete function.
 * boolean pointer is for efficiency; deleting a node requires checking
 * if it has children, which takes O(NUMCHAR) time. By marking if a
 * deletion has occurred or not we can skip calling node_has_children
 * unnecessarily
 */
Node* radix_delete_rec(Node* node, char* word, bool* deleted)
{
   if (node == NULL) return node;

   int nodePrefixLength = strlen(node->prefix);
   if (strlen(word) < nodePrefixLength || strncmp(node->prefix, word, nodePrefixLength) != 0)
   {
      /** First condition checks if the word is shorter than node->prefix,
       * and the second condition actually makes sure that word and node->prefix
       * are the same if they happen to be the same length */
      return node;
   }

   word += nodePrefixLength;  // Increment pointer by the length of node->prefix

   if (*word == '\0')
   {
      if (node->isEndOfWord)
      {
         node->isEndOfWord = false; /** If the node actually contains a word then delete it by
                                       marking it as not a word */
         *deleted = true;           /** Mark that a deletion has occurred */

         if (!nodeHasChildren(node))
         {
            // If the node is a leaf then deleting it immediately is possible
            free(node->prefix);
            free(node);
            node = NULL;
         }
      }
      return node;
   }

   unsigned char index = (unsigned char)word[0];

   node->child[index] = radix_delete_rec(node->child[index], word, deleted);  // Recursively go down the tree

   if (*deleted && !nodeHasChildren(node) && !node->isEndOfWord)
   {
      /**
       * If a deletion has occurred then we need to check if the node is a leaf and
       * not the end of a word
       */
      free(node->prefix);
      free(node);
      node = NULL;
   }
   return node;
}

// Wrapper function for radix_delete
bool radix_delete(Node** root, char* word)
{
   if (root == NULL || *root == NULL || !hasValidCharacters(word) || strlen(word) == 0) return false;

   bool result = false;

   *root = radix_delete_rec(*root, word, &result);
   return result;
}

int main()
{
   RadixTree tree;
   tree.root = NULL;

   // radix_insert(&tree.root, "cart");
   radix_insert(&tree.root, "carp");
   // radix_insert(&tree.root, "car");
   // radix_insert(&tree.root, "came");

   // FILE* fp = fopen("dictionary.txt", "r");
   // if (NULL == fp)
   // {
   //    fprintf(stderr, "Error while opening dictionary file");
   //    exit(1);
   // }

   // int ret;
   // char word[100] = {0};
   // // insert all the words from the dictionary
   // while (fgets(word, sizeof(word), fp))
   // {
   //    word[strcspn(word, "\r\n")] = '\0';  // Remove a line ending if present
   //    radix_insert(&tree.root, word);
   // }

   while (1)
   {
      char word[100] = {0};
      printf("Enter keyword: ");
      if (1 != scanf("%99s", word))
      {
         break;
      }

      radix_print(tree.root, word);
      radix_delete(&tree.root, word);
   }

   return 0;
}
