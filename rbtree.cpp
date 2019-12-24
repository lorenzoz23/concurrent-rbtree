#include <iostream>
#include <queue>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <pthread.h>
#include <algorithm>

// used to make a colorful command-line user interface
#include "colors.h"

using namespace std;

// enum representing a node's color - red or black (0 or 1)
enum Color { red, black };

/**
 * Represents a standard node in a 
 * red-black tree.
 */
struct node {
  int key;
  node* parent;
  node* left;
  node* right;
  bool color;

  node(int key) 
  { 
    this->key = key; 
    left = right = parent = NULL; 
    this->color = red; 
  } 
};
typedef node* node_p;

/**
 * After reading the input file,
 * temporary nodes are created with just a 
 * key and color before being used to create the
 * initial tree.
 */
struct tmp_node {
  int key;
  bool color;
};
typedef tmp_node* tmp_node_p;

/**
 * Represents the sequences of invocations
 * read in from the input file; each has an 
 * operation (search) and a key (7).
 */
struct tree_op {
  string operation;
  int key;
};
typedef tree_op* t_op;

/**
 * This is fed into the method that creates the 
 * output file; it represents the results from the 
 * input file.
 */
struct results {
  double time;
  vector<int> search_true;
  vector<long> search_thread_ids;
  string final_rbt;
};
typedef results* results_p;

/**
 * Monitor class for the readers-writers problem
 * that is used to implement concurrency in this project.
 * It includes standard variables and methods associated with
 * a readers-writers problem where readers have priority.
 */
class rw_monitor {
private:
  int num_readers;
  int num_writers;
  int readers_wait;
  int writers_wait;

  pthread_cond_t can_read;
  pthread_cond_t can_write;
  pthread_mutex_t cond_lock;

public:
  rw_monitor()
  {
    num_readers = 0;
    num_writers = 0;
    readers_wait = 0;
    writers_wait = 0;

    pthread_cond_init(&can_read, NULL); 
    pthread_cond_init(&can_write, NULL); 
    pthread_mutex_init(&cond_lock, NULL);
  }

  /**
   * This function begins the reading of 
   * tree data if there is not a writer already
   * locking the tree. There can be multiple readers.
   */
  void begin_read(int reader)
  {
    pthread_mutex_lock(&cond_lock);
    
    if (num_writers == 1 || writers_wait > 0) { 
      readers_wait++;
      
      pthread_cond_wait(&can_read, &cond_lock); 
      readers_wait--; 
    }
    
    num_readers++;
    pthread_mutex_unlock(&cond_lock); 
    pthread_cond_broadcast(&can_read);
  }

  /**
   * This function ends a reader and checks if 
   * there are any left. If there are none left, 
   * then any waiting writers can begin inserting/deleting.
   */
  void end_read(int reader)
  {
    pthread_mutex_lock(&cond_lock); 
    
    if (num_readers-- == 0) { 
      pthread_cond_signal(&can_write);
    }
    
    pthread_mutex_unlock(&cond_lock); 
  }

  /**
   * This function begins the inserting/deleting of
   * the global red-black tree if there are no readers
   * currently searching the tree.
   */
  void begin_write(int writer)
  {
    pthread_mutex_lock(&cond_lock); 
    
    if (num_writers == 1 || num_readers > 0) { 
      writers_wait++; 
      pthread_cond_wait(&can_write, &cond_lock); 
      writers_wait--; 
    } 
    num_writers = 1;
    pthread_mutex_unlock(&cond_lock); 
  }

  /**
   * This function ends a writer and
   * unblocks any waiting readers so they
   * can begin their searches of the tree.
   */
  void end_write(int writer)
  {
    pthread_mutex_lock(&cond_lock); 
    num_writers = 0; 
    
    if (readers_wait > 0) { 
      pthread_cond_signal(&can_read);
    }
    else {
      pthread_cond_signal(&can_write);
    }
    pthread_mutex_unlock(&cond_lock);
  }
};

/**
 * This class represents a red black tree.
 * It includes functions for inserting, deleting,
 * searching, and printing the tree in different ways.
 */
class RBTree {
public:
  RBTree() { root = NULL; }
  RBTree(vector<tmp_node_p> t_rbt)
  {
    tmp_tree = t_rbt;
    root = NULL;
  }

  void build_tree();
  void insert_node(int key);
  void delete_node(int key);
  node_p search_tree(int key);
  void* search_thread(void* key);
  node_p get_root() { return root; }
  void in_order();
  void level_order();
  void prefix_order();
  void print_tree();
  string get_prefix_tree();
private:
  node_p root;
  tmp_node_p last;
  vector<tmp_node_p> tmp_tree;
  string prefix_tree;
  
  node_p insert_helper(node_p root, node_p node);
  node_p build_tree_helper(node_p root, tmp_node_p curr);
  void fix_insert(node_p& root, node_p& node);
  void fix_double_black(node_p node);
  void rotate_right(node_p& root, node_p& node);
  void rotate_left(node_p& root, node_p& node);
  void in_order_helper(node_p root);
  void prefix_order_helper(node_p root);
  void level_order_helper(node_p root);
  void delete_helper(node_p node);
  void print_tree_helper(node_p root, string delimiter, bool last);
  node_p search_helper(node_p node, int key);
  node_p min(node_p node);
  node_p replace(node_p node);
  bool is_on_left(node_p node);
  node_p sibling(node_p node);
  bool red_child(node_p node);
  void swap_keys(node_p u, node_p v);
};

/**
 * This is the struct that is 
 * passed into the thread functions since
 * they can accept only one argument.
 */
struct thread_data {
  RBTree rbt;
  t_op op;
  results_p results;
  int tid;
};
typedef thread_data* t_data;

/**
 * Helper function that prints out the tree
 * in a skeleton-like manner showing the connections
 * between all nodes.
 */
void RBTree::print_tree_helper(node_p root, string delimiter, bool last)
{
  if(root != NULL) {
    cout << delimiter;

    if(last) {
      cout << "R----";
      delimiter += "    ";
    }
    else {
      cout << "L----";
      delimiter += "|   ";
    }

    string c = root->color ? "BLACK" : "RED";
    cout << root->key << "(" << c << ")" << endl;
    print_tree_helper(root->left, delimiter, false);
    print_tree_helper(root->right, delimiter, true);
  }
}

/**
 * Helper function for printing out the 
 * tree in an in-order fashion.
 */
void RBTree::in_order_helper(node_p root)
{
  if(root == NULL) return;

  in_order_helper(root->left);
  cout << root->key << " ";
  in_order_helper(root->right);
}

/**
 * Helper function for printing out 
 * the tree in a level-order fashion.
 */
void RBTree::level_order_helper(node_p root)
{
  if(root == NULL) return;
  
  queue<node_p> q;
  q.push(root);

  while(!q.empty()) {
    node_p temp = q.front();
    cout << temp->key << " ";
    q.pop();

    if(temp->left != NULL) {
      q.push(temp->left);
    }
    if(temp->right != NULL) {
      q.push(temp->right);
    }
  }
}

/**
 * Used to get the final red-black tree string.
 * RETURNS the final red-black tree in prefix order 
 * as a string 
 */
string RBTree::get_prefix_tree()
{
  return prefix_tree;
}

/**
 * Helper function for building the final red
 * black tree in prefix order as a string.
 */
void RBTree::prefix_order_helper(node_p node)
{
  if(node == NULL) return;
  char c = node->color ? 'b' : 'r';
  prefix_tree += to_string(node->key) + c + ",";
  if(node->left == NULL) prefix_tree += "f,";
  if(node->right == NULL && last->key != node->key) prefix_tree += "f,";

  prefix_order_helper(node->left);
  prefix_order_helper(node->right);
}

/**
 * Prints out the tree in-order.
 */
void RBTree::in_order()
{
  in_order_helper(root);
}

/**
 * Prints out the tree in level-order.
 */
void RBTree::level_order()
{
  level_order_helper(root);
}

/**
 * Builds the final red-black tree as a string
 * in prefix order.
 */
void RBTree::prefix_order() {
  prefix_tree = "";
  prefix_order_helper(root);
}

/**
 * Helper function used to insert a node into the tree.
 * RETURNS the root of the tree.
 */
node_p RBTree::insert_helper(node_p root, node_p node)
{
  if(root == NULL) return node;

  if(node->key < root->key) {
    root->left = insert_helper(root->left, node);
    root->left->parent = root;
  }
  else if(node->key > root->key) {
    root->right = insert_helper(root->right, node);
    root->right->parent = root;
  }

  return root;
}

/**
 * Fixes any inconsistencies caused by deleting a node
 * from the tree.
 */
void RBTree::fix_double_black(node_p node)
{
  if(node == root) return;

  node_p sib = sibling(node), parent = node->parent;
  if(sib == NULL) {
    fix_double_black(parent);
  }
  else {
    if(sib->color == red) {
      parent->color = red;
      sib->color = black;
      if(is_on_left(sib)) {
	rotate_right(root, parent);
      }
      else {
	rotate_left(root, parent);
      }
      fix_double_black(node);
    }
    else {
      if(red_child(sib)) {
	if(sib->left != NULL && sib->left->color == red) {
	  if(is_on_left(sib)) {
	    sib->left->color = sib->color;
	    sib->color = parent->color;
	    rotate_right(root, parent);
	  }
	  else {
	    sib->left->color = parent->color;
	    rotate_right(root, sib);
	    rotate_left(root, parent);
	  }
	}
	else {
	  if(is_on_left(sib)) {
	    sib->right->color = parent->color;
	    rotate_left(root, sib);
	    rotate_right(root, parent);
	  }
	  else {
	    sib->right->color = sib->color;
	    sib->color = parent->color;
	    rotate_left(root, parent);
	  }
	}
	parent->color = black;
      }
      else {
	sib->color = red;
	if(parent->color == black) {
	  fix_double_black(parent);
	}
	else {
	  parent->color = black;
	}
      }
    }
  }
}

/**
 * RETURNS true if node has a red child, false otherwise.
 */
bool RBTree::red_child(node_p node)
{
  return (node->left != NULL && node->left->color == red) ||
    (node->right != NULL && node->right->color == red);
}

/**
 * RETURNS true if node is on the 
 * left side of its parent, false otherwise.
 */
bool RBTree::is_on_left(node_p node)
{
  return node == node->parent->left;
}

/**
 * RETURNS the node's sibling if 
 * it has one, NULL node otherwise.
 */
node_p RBTree::sibling(node_p node)
{
  if(node->parent == NULL) {
    return NULL;
  }
  if(is_on_left(node)) {
    return node->parent->right;
  }
  return node->parent->left;
}

/**
 * RETURNS the min node of the tree.
 */
node_p RBTree::min(node_p node)
{
  node_p tmp = node;
  while(tmp->left != NULL) {
    tmp = tmp->left;
  }

  return tmp;
}

/**
 * Function for swapping the keys
 * of two nodes.
 */
void RBTree::swap_keys(node_p u, node_p v)
{
  int tmp;
  tmp = u->key;
  u->key = v->key;
  v->key = tmp;
}

/**
 * This function finds the node that is
 * to replace the deleted node in the tree.
 * RETURNS the node that will replace the node 
 * to be deleted in the tree
 */
node_p RBTree::replace(node_p node)
{
  if(node->left != NULL && node->right != NULL) {
    return min(node->right);
  }
  if(node->left == NULL && node->right == NULL) {
    return NULL;
  }
  if(node->left != NULL) {
    return node->left;
  }
  else {
    return node->right;
  }
}

/**
 * Deletes a node from the tree 
 * with the given key.
 */
void RBTree::delete_helper(node_p n)
{
  node_p m = replace(n);

  bool bb = ((m == NULL || m->color == black) && (n->color == black));
  node_p parent = n->parent;

  if(m == NULL) {
    if(n == root) {
      root = NULL;
    }
    else {
      if(bb) {
	fix_double_black(n);
      }
      else {
	if(sibling(n) != NULL) {
	  sibling(n)->color = red;
	}
      }

      if(is_on_left(n)) {
	parent->left = NULL;
      }
      else {
	parent->right = NULL;
      }
    }
    delete n;
    return;
  }

  if(n->left == NULL || n->right == NULL) {
    if(n == root) {
      n->key = m->key;
      n->left = n->right = NULL;
      delete m;
    }
    else {
      if(is_on_left(n)) {
	parent->left = m;
      }
      else {
	parent->right = m;
      }
      delete n;

      m->parent = parent;
      if(bb) {
	fix_double_black(m);
      }
      else {
	m->color = black;
      }
    }
    return;
  }

  swap_keys(m, n);
  delete_helper(m);
}

/**
 * Helper function that perfoms a binary search of the 
 * red-black tree to try and find the node with the given key.
 * RETURNS the found node, NULL otherwise.
 */
node_p RBTree::search_helper(node_p node, int key)
{
  if(node == NULL || key == node->key) {
    return node;
  }

  if(key < node->key) {
    return search_helper(node->left, key);
  }

  return search_helper(node->right, key);
}

/**
 * Rotates the tree right if there is inconsistencies
 * in the structural integrity of the red-black tree.
 */
void RBTree::rotate_right(node_p& root, node_p& node)
{
  node_p left = node->left;
  node->left = left->right;

  if(node->left != NULL) {
    node->left->parent = node;
  }

  left->parent = node->parent;

  if(node->parent == NULL) {
    root = left;
  }
  else if(node == node->parent->left) {
    node->parent->left = left;
  }
  else {
    node->parent->right = left;
  }

  left->right = node;
  node->parent = left;
}

/**
 * Rotates the tree left if there is inconsistencies
 * in the structural integrity of the red-black tree.
 */
void RBTree::rotate_left(node_p& root, node_p& node)
{
  node_p right = node->right;
  node->right = right->left;

  if(node->right != NULL) {
    node->right->parent = node;
  }

  right->parent = node->parent;

  if(node->parent == NULL) {
    root = right;
  }
  else if(node == node->parent->left) {
    node->parent->left = right;
  }
  else {
    node->parent->right = right;
  }

  right->left = node;
  node->parent = right;
}

/**
 * Helper function used to re-balance the 
 * tree after an insert.
 */
void RBTree::fix_insert(node_p& root, node_p& node)
{
  node_p parent = NULL;
  node_p grand_parent = NULL;

  while((node != root) && (node->color != black) &&
	(node->parent->color == red)) {
    parent = node->parent;
    grand_parent = node->parent->parent;

    if(parent == grand_parent->left) {
      node_p uncle = grand_parent->right;

      if(uncle != NULL && uncle->color == red) {
	grand_parent->color = red;
	parent->color = black;
	uncle->color = black;
	node = grand_parent;
      }
      else {
	if(node == parent->right) {
	  rotate_left(root, parent);
	  node = parent;
	  parent = node->parent;
	}

	rotate_right(root, grand_parent);
	parent->color = black;
	grand_parent->color = red;
	node = parent;
      }
    }
    else {
      node_p uncle = grand_parent->left;

      if((uncle != NULL) && (uncle->color == red)) {
	grand_parent->color = red;
	parent->color = black;
	uncle->color = black;
	node = grand_parent;
      }
      else {
	if(node == parent->left) {
	  rotate_right(root, parent);
	  node = parent;
	  parent = node->parent;
	}

	rotate_left(root, grand_parent);
	parent->color = black;
	grand_parent->color = red;
	node = parent;
      }
    }
  }

  root->color = black;
}

/**
 * Inserts a node with the given key
 * into the tree.
 */
void RBTree::insert_node(int key)
{
  if(search_tree(key) == NULL) {
    node_p npt = new node(key);
    
    root = insert_helper(root, npt);
    
    fix_insert(root, npt);
  }
}

/**
 * Deletes a node from the tree
 * with the given key.
 */
void RBTree::delete_node(int key)
{
  if(root == NULL) return;

  node_p n = search_tree(key);

  if(n == NULL) {
    cout << "Error: couldn't find " << key << "in the tree." << endl;
    return;
  }
  
  delete_helper(n);
}

/**
 * Searches the tree for a node
 * with the given key.
 */
node_p RBTree::search_tree(int key)
{
  return search_helper(root, key);
}

/**
 * Prints the skeleton of the tree.
 */
void RBTree::print_tree()
{
  if(root != NULL) {
    print_tree_helper(this->root, "", true);
  }
}

/**
 * Helper function that helps build the initial 
 * tree in prefix order from the given input file
 * in a binary search tree fashion.
 * RETURNS a new node if n happens to be null.
 */
node_p RBTree::build_tree_helper(node_p n, tmp_node_p curr)
{
  if(n == NULL) {
    n = new node(curr->key);
    n->color = curr->color;
  }
  else if(curr->key > n->key) {
    n->right = build_tree_helper(n->right, curr);
    n->right->parent = n;
  }
  else {
    n->left = build_tree_helper(n->left, curr);
    n->left->parent = n;
  }
  return n;
}

/**
 * Builds the initial red-black tree in 
 * prefix order as a binary search tree since
 * the input file is said to always be a valid red
 * black tree.
 */
void RBTree::build_tree()
{
  vector<tmp_node_p>::const_iterator itv;
  itv = tmp_tree.begin();
  
  tmp_node_p root_tmp = *itv;
  root = build_tree_helper(root, root_tmp);
  itv++;
  while(itv != tmp_tree.end()) {
    tmp_node_p curr = *itv;
    if(curr->key != -1) {
      last = curr;
      build_tree_helper(root, curr);
    }
    itv++;
  }
}

/**
 * Class representing all the input/output
 * procedures of the project. This includes 
 * functions for readind the input file and creating
 * the output file, as well as parsing.
 */
class IO {
public:
  vector<tmp_node_p> tree;
  vector<int> worker_threads;
  vector<t_op> searchers;
  vector<t_op> modifiers;

  void parse_tree_line(string line);
  void parse_thread_lines(string lines);
  void parse_invocation_lines(string lines);
  void parse_input_file(string filename);
  void write_output(string output_filename, results_p results);
};

/**
 * Parses the first line of the input file
 * containing the tree in prefix order.
 */
void IO::parse_tree_line(string line)
{
  stringstream ss(line);

  while(ss.good()) {
    tmp_node_p n = new tmp_node;
    string s;
    getline(ss, s, ',');
    if(s[s.length() - 1] == '\n') {
      s = s.substr(0, s.length() - 1);
    }
    
    if(s[0] == 'f') {
      n->color = black;
      n->key = -1;
    }
    else {
      if(s[s.length() - 1] == 'b') {
	n->color = black;
      }
      else if(s[s.length() - 1] == 'r') {
	n->color = red;
      }
      s[s.length() - 1] = '\0';
      n->key = stoi(s);
    }
    tree.push_back(n);
  }
}

/**
 * Parses the thread lines that contain the 
 * number of search and modify threads.
 */
void IO::parse_thread_lines(string lines)
{
  stringstream ss(lines);
  int search = 0;
  int modify = 0;
  vector<int> v;
  
  while(ss.good()) {
    string curr;
    getline(ss, curr, '\n');
    if(curr.length() == 0) break;
    string first = curr.substr(0, curr.find(" "));
    for(string::size_type i = 0; i < first.length(); i++) {
      if(isupper(first[i])) {
	first[i] = first[i] + 32;
      }
    }

    int num = 0;
    if(curr[curr.length() - 3] == ' ') {
      string s = curr.substr(curr.length() - 2);
      num = stoi(s);
    }
    else if(curr[curr.length() - 2] == ' ') {
      num = curr[curr.length() - 1] - '0';
    }
    
    if(first == "search") {
      search = num;
    }
    else {
      modify = num;
    }
  }

  v.push_back(search);
  v.push_back(modify);
  worker_threads = v;
}

/**
 * Parses the invocation lines that contain
 * all the search, insert, and delete operations
 * that will be perfomed on the tree.
 */
void IO::parse_invocation_lines(string lines)
{
  vector<t_op> s_op_v;
  vector<t_op> m_op_v;
  stringstream ss(lines);
  string op, line, curr;
  int key = 0;

  while(ss.good()) {
    getline(ss, line, '\n');
    if(line.length() == 0) break;
    stringstream ss2(line);
    while(ss2.good()) {
      t_op invo = new tree_op;
      getline(ss2, curr, '|');
      if(curr.length() == 0) break;
      if(curr[0] == ' ') curr = curr.substr(1);
      op = curr.substr(0, curr.find("("));
      invo->operation = op;
      key = stoi(curr.substr(op.length() + 1, curr.find(")")));
      invo->key = key;
      if(invo->operation == "search") s_op_v.push_back(invo);
      else m_op_v.push_back(invo);
      getline(ss2, curr, '|');
    }
  } 
  
  searchers = s_op_v;
  modifiers = m_op_v;
}

/**
 * Parent parser function that calls upon 
 * the above three to parse the input file.
 */
void IO::parse_input_file(string filename)
{
  string curr, s;
  int num_blanks = 0;
  ifstream file(filename);

  if(file.is_open()) {
    while(getline(file, curr)) {
      if(curr == "") {
	num_blanks++;
	if(num_blanks == 1) {
	  parse_tree_line(s);
	}
	else if(num_blanks == 2) {
	  parse_thread_lines(s);
	}
	s = "";
	continue;
      }
      else s += curr + "\n";
    }
    parse_invocation_lines(s);
    file.close();
  }
  else {
    cout << "Error: unable to open input file!" << endl;
    exit(1);
  }
}

/**
 * Creates the output file and writes it to disk.
 * The output includes the overall execution time
 * of all the invocations, the results from all the searches,
 * and the final red black tree in prefix order.
 */
void IO::write_output(string output_filename, results_p r)
{
  ofstream file(output_filename);
  int count = 0;

  if(file.is_open()) {
    file << "Execution time: " << endl;
    file << r->time << " seconds" << endl;
    file << endl;
    file << "Search output: " << endl;

    if(searchers.size() > (unsigned)worker_threads[0]) count = worker_threads[0];
    else count = searchers.size();
      
      for(int i = 0; i < count; i++) {
      string s;
      if(find(r->search_true.begin(), r->search_true.end(), searchers[i]->key) != r->search_true.end()) {
	s = "true";
      }
      else s = "false";
      file << searchers[i]->operation << "(" << searchers[i]->key << ")->" << s << ", performed by thread: " << r->search_thread_ids[i];
      file << endl;
    }
    file << endl;
    file << "Final Red-Black Tree: " << endl;
    file << r->final_rbt << endl;
    
    file.close();
  }
  else {
    cout << "Error: unable to open file!" << endl;
    exit(1);
  }
}

/**
 * Thread function that searches the tree
 * for a node with a given key.
 */
void* search_thread(void* thread_data)
{
  t_data data;
  data = (t_data) thread_data;
  if(data->rbt.search_tree(data->op->key) != NULL) {
    data->results->search_true.push_back(data->op->key);
  }
  data->results->search_thread_ids.push_back((long) pthread_self());
  return NULL;
}

/**
 * Thread function that inserts a new node
 * into the tree with a given key.
 */
void* insert_thread(void* thread_data)
{
  t_data data;
  data = (t_data) thread_data;
  data->rbt.insert_node(data->op->key);
  return NULL;
}

/**
 * Thread function that deletes a node
 * from the tree with a given key.
 */
void* delete_thread(void* thread_data)
{
  t_data data;
  data = (t_data) thread_data;
  data->rbt.delete_node(data->op->key);
  return NULL;
}

// global rw_monitor class object
rw_monitor M;

/**
 * Reader function that is used
 * for concurrent searches to the tree
 * using monitors.
 * RETURNS a void pointer
 */
void* reader(void* reader_data)
{
  t_data data;
  data = (t_data) reader_data;
  M.begin_read(data->tid);
  search_thread(data);
  M.end_read(data->tid);
  return NULL;
}

/**
 * Writer function that alters 
 * the contents of the tree if there 
 * are no readers searching through the tree.
 * RETURNS a void pointer
 */
void* writer(void* writer_data)
{
  t_data data;
  data = (t_data) writer_data;
  M.begin_write(data->tid);
  if(data->op->operation == "insert") {
    insert_thread(data);
  }
  else delete_thread(data);
  M.end_write(data->tid);
  return NULL;
}

/**
 * Main function that outputs a command-line
 * user interface to the user with cool colors.
 * Performs the parsing and building of the tree
 * and all the thread creation.
 */
int main(int argc, char* argv[])
{
  string filename, output_filename;
  IO io;

  cout << "----------------------------------------" << endl;
  cout << FYEL("PROJECT") ": CONCURRENT RED - BLACK TREES\n" FGRN("CLASS")
    ": COM S 352\n" FBLU("AUTHOR") ": LORENZO ZENITSKY" << endl;
  cout << "----------------------------------------\n" << endl;

  if(!argv[1]) {
    cout << FRED("ERROR") ": Please specify an input file for the program to read!" << endl;
    exit(EXIT_FAILURE);
  }

  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
  filename = argv[1];
  io.parse_input_file(filename);

  RBTree rbt(io.tree);
  rbt.build_tree();

  queue<t_op> search;
  queue<t_op> modify;
  results_p r = new results;

  int NUM_THREADS = io.worker_threads[0] + io.worker_threads[1];
  pthread_t threads[NUM_THREADS];

  for(unsigned i = 0; i < io.searchers.size(); i++) {
    search.push(io.searchers[i]);
  }
  for(unsigned i = 0; i < io.modifiers.size(); i++) {
    modify.push(io.modifiers[i]);
  }

  int s_thread_count = 0;
  while(!search.empty() && s_thread_count < io.worker_threads[0]) {
    t_op tmp = search.front();
    t_data data = new thread_data;
    data->op = tmp;
    data->rbt = rbt;
    data->results = r;
    data->tid = s_thread_count;

    if(pthread_create(&threads[s_thread_count], NULL, reader, data)) {
      cout << "Error: unable to create search thread " << s_thread_count << endl;
    }

    s_thread_count++;
    search.pop();
  }
  
  for (int i = 0; i < s_thread_count; i++) { 
    pthread_join(threads[i], NULL); 
  }

  int m_thread_count = s_thread_count;
  while(!modify.empty() && ((m_thread_count - s_thread_count) < io.worker_threads[1])) {
    t_op tmp = modify.front();
    t_data data = new thread_data;
    data->op = tmp;
    data->rbt = rbt;
    data->results = r;
    data->tid = m_thread_count;

    if(tmp->operation == "insert") {
      if(pthread_create(&threads[m_thread_count], NULL, writer, data)) {
	cout << "Error: unable to create insert thread " << m_thread_count << endl;
      }
    }
    else {
      if(pthread_create(&threads[m_thread_count], NULL, writer, data)) {
	cout << "Error: unable to create delete thread " << m_thread_count << endl;
      }
    }

    pthread_join(threads[m_thread_count], NULL);
    m_thread_count++;
    modify.pop();
  }

  rbt.prefix_order();
  r->final_rbt = rbt.get_prefix_tree();
  std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
  
  cout << FCYN("Filename") ": " << filename << endl << endl;
  cout << "Please enter a name for the output file: ";
  cin >> output_filename;
  cout << endl;
  
  std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
  r->time = time_span.count();
  io.write_output(output_filename, r);
  
  cout << "The following has just been written to the output file, "<< output_filename << "." << endl << endl;
  cout << "* " << FBLU("Execution time") << endl;
  cout << "* " << FGRN("The output of each search operation, if any") << endl;
  cout << "* " << FYEL("The final red-black tree") << endl << endl;

  cout << FRED("Goodbye") << "..." << endl;
  return 0;
}
