/*
 * TAR File-system Driver
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (4)
 */

/*
 * STUDENT NUMBER: s1820742
 */
#include "tarfs.h"
#include <infos/kernel/log.h>

using namespace infos::fs;
using namespace infos::drivers;
using namespace infos::drivers::block;
using namespace infos::kernel;
using namespace infos::util;
using namespace tarfs;

/**
 * TAR files contain header data encoded as octal values in ASCII.  This function
 * converts this terrible representation into a real unsigned integer.
 *
 * You DO NOT need to modify this function.
 *
 * @param data The (null-terminated) ASCII data containing an octal number.
 * @return Returns an unsigned integer number, corresponding to the input data.
 */
static inline unsigned int octal2ui(const char *data)
{
	// Current working value.
	unsigned int value = 0;

	// Length of the input data.
	int len = strlen(data);

	// Starting at i = 1, with a factor of one.
	int i = 1, factor = 1;
	while (i < len) {
		// Extract the current character we're working on (backwards from the end).
		char ch = data[len - i];

		// Add the value of the character, multipled by the factor, to
		// the working value.
		value += factor * (ch - '0');
		
		// Increment the factor by multiplying it by eight.
		factor *= 8;
		
		// Increment the current character position.
		i++;
	}

	// Return the current working value.
	return value;
}

// The structure that represents the header block present in
// TAR files.  A header block occurs before every file, this
// this structure must EXACTLY match the layout as described
// in the TAR file format description.
namespace tarfs {
	struct posix_header {
		// path/filename
        char name[100];               /*   0 */
        char mode[8];                 /* 100 */
        char uid[8];                  /* 108 */
        char gid[8];                  /* 116 */

        // File size
        char size[12];                /* 124 */
        char mtime[12];               /* 136 */
        char chksum[8];               /* 148 */
        char typeflag;                /* 156 */
        char linkname[100];           /* 157 */
        char magic[6];                /* 257 */
        char version[2];              /* 263 */
        char uname[32];               /* 265 */
        char gname[32];               /* 297 */
        char devmajor[8];             /* 329 */
        char devminor[8];             /* 337 */
        char prefix[155];             /* 345 */
        //why not 162?                        /* 500 */

	} __packed;
}

/**
 * Reads the contents of the file into the buffer, from the specified file offset.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @param off The offset within the file.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::pread(void* buffer, size_t size, off_t off)
{
	if (off >= this->size()) return 0;
	// If
	if (off + size > this->size()){
	    size = this->size() - off;
	}
    int block_size = _owner.block_device().block_size();

    int block_number = size/block_size;
    block_number = size % block_size ? block_number++:block_number;
	char* content = new char[block_number * block_size];
	int start_block = off/block_size;
    _owner.block_device().read_blocks(content, _file_start_block+start_block, block_number);
//    char* content_copy = new char[size];
//    content_copy = content + off % block_size;
//    //
//    buffer = content_copy;
    while(size>0){
        *buffer++ = *(content++ +off%block_size);
        size--;
    }

	// buffer is a pointer to the buffer that should receive the data.
	// size is the amount of data to read from the file.
	// off is the zero-based offset within the file to start reading from.
    // reads count blocks into buffer, starting from block offset.
	
	return size;
}


/**
 * Reads all the file headers in the TAR file, and builds an in-memory
 * representation.
 * @return Returns the root TarFSNode that corresponds to the TAR file structure.
 */
//TarFSNode* TarFS::build_tree()
//{
//    // Create the root node.
//    TarFSNode *root = new TarFSNode(NULL, "", *this);
//
//    // TO BE FILLED IN
//    int file_number;
//    int block_number = block_device().block_count();
//    struct posix_header *file_hdr = (struct posix_header *) new char [_owner.block_device().block_size()];
//    String directory = container[0:100];
//    // entity/TarFSNode = root
//    while (True){
//    }
//}
//    for(int i = 0; i < file_number; i++){
//        //let *file_hdr point to the current file
//        //file_hdr->name split into String[]
//        //if(entity.children.names ! contain String[j]){
//        //  for(int j = 0; j < len(String[])-1; j++){
//        //      create an file entity which is a TarFSNode names String[j]
//        //      entity.parent = entity
//        //      initialise entity = the struct which has name String[path/j]
//        //  }
//        //}else{
//
//    //}
//    //entity.child = TarFSNode(*file_hdr)
//    //
//
//    // You must read the TAR file, and build a tree of TarFSNodes that represents each file present in the archive.
//
//    return root;
//}

TarFSNode* TarFS::build_tree()
{
    // Create the root node.
    TarFSNode *root = new TarFSNode(NULL, "", *this);

    // Initialises the file header, file path and file name to begin analysing Tar nodes
    posix_header *file_hdr = (posix_header *) new char[block_device().block_size()];
    String file_path;
    String file_name;
    unsigned int file_size = 0;
    size_t nr_blocks = block_device().block_count();
    //mm_log.messagef(LogLevel::IMPORTANT, "str %d",nr_blocks);

    // Loops through the headers while the offset index is less that the total blocks
    for (unsigned int off = 0; off < nr_blocks; off+=file_size+1){
        TarFSNode *lead = root;
        block_device().read_blocks(file_hdr, off, 1);

        // Check if the zero block is present in the file header which shows the archive end
        // then break
        if (is_zero_block((uint8_t *) file_hdr)) {
            uint8_t *block_zero = new uint8_t[block_device().block_size()];
            block_device().read_blocks(block_zero, off + 1, 1);

            // Checks if the and offset adjusted block_zero is the block with the archive end and breaks the loop if so.
            // Deallocates the memory for this either way to avoid memory leaks
            if (is_zero_block(block_zero)) {
                delete(block_zero);
                break;
            }
            delete(block_zero);
        }

        // Split the full path into a list of parts of string and updates the file_name as the last element of the list
        // if the path is not empty
        file_path = String(file_hdr->name);
        List <String> file_path_parts = file_path.split('/', true);
        //mm_log.messagef(LogLevel::IMPORTANT, "file path %s",file_path.c_str());

        if (!file_path_parts.empty())
            file_name = file_path_parts.last();
        else
            return root;

        // If the file path is greater than one
        // that the file is in a lower hierarchy directory than
        // the current one
        if (file_path_parts.count() > 1) {

            String file_directory = file_path_parts.at(file_path_parts.count() - 2);
            String lead_node_name = (String) lead->name();

            TarFSNode *temp_node = root;

            // Sets the updates the lead_node and its name if the name of the file_directory and the name of the
            // lead_node are different
            for(unsigned int i =0; i< file_path_parts.count() - 1;i++)
            {
                String cur = file_path_parts.at(i);
                if(!temp_node->get_child(cur))
                {
                    TarFSNode *new_node = new TarFSNode(temp_node, cur, *this);
                    new_node->set_block_offset(off);
                    new_node->size(0);//size 0 as this is not a file, its a folder, internal node within the tree
                    temp_node->add_child(cur,new_node);
                }
                temp_node = (TarFSNode *) temp_node->get_child(cur);

            }
            lead = temp_node;
            lead_node_name = String(temp_node->name());
        }
        else if (file_path_parts.count() == 1) {
            // Case where the file is in the same directory hierarchy as the current one
            lead = root;
            lead->size(octal2ui(file_hdr->size));

            //mm_log.messagef(LogLevel::IMPORTANT, "lead_node_name %s, file size %d",file_path_parts.at(0).c_str(), octal2ui(file_hdr->size));
        }

        // Updates the file size accounting for an extra beginning or end section
        file_size = octal2ui(file_hdr->size);
        unsigned int temp = file_size;
        file_size = (file_size % BLOCKSIZE) ? (file_size/BLOCKSIZE + 1) : file_size/BLOCKSIZE;

        // Builds the tree by initialising and adding the current positioned node to the lead_node as a child
        TarFSNode *cur_node = new TarFSNode(lead, file_name, *this);
        cur_node->set_block_offset(off);
        cur_node->size(temp);

        // Add the child node
        lead->add_child(file_name, cur_node);

        // Updates the lead node when the last point in the path is reached to next directory to search for files
        if (file_path.c_str()[file_path.length() - 1] == '/')
            lead = cur_node;
    }
    //mm_log.messagef(LogLevel::IMPORTANT, "str %d",block_device().block_count());
    return root;
}



/**
 * Returns the size of this TarFS File
 */
unsigned int TarFSFile::size() const
{
    return octal2ui(_hdr->size) ;
}

/* --- YOU DO NOT NEED TO CHANGE ANYTHING BELOW THIS LINE --- */

/**
 * Mounts a TARFS filesystem, by pre-building the file system tree in memory.
 * @return Returns the root node of the TARFS filesystem.
 */
PFSNode *TarFS::mount()
{
	// If the root node has not been generated, then build it.
	if (_root_node == NULL) {
		_root_node = build_tree();
	}

	// Return the root node.
	return _root_node;
}

/**
 * Constructs a TarFS File object, given the owning file system and the block
 */
TarFSFile::TarFSFile(TarFS& owner, unsigned int file_header_block)
: _hdr(NULL),
_owner(owner),
_file_start_block(file_header_block),
_cur_pos(0)
{
	// Allocate storage for the header.
	_hdr = (struct posix_header *) new char[_owner.block_device().block_size()];
	
	// Read the header block into the header structure.
	_owner.block_device().read_blocks(_hdr, _file_start_block, 1);
	
	// Increment the starting block for file data.
	_file_start_block++;
}

TarFSFile::~TarFSFile()
{
	// Delete the header structure that was allocated in the constructor.
	delete _hdr;
}

/**
 * Releases any resources associated with this file.
 */
void TarFSFile::close()
{
	// Nothing to release.
}

/**
 * Reads the contents of the file into the buffer, from the current file offset.
 * The current file offset is advanced by the number of bytes read.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::read(void* buffer, size_t size)
{
	// Read can be seen as a special case of pread, that uses an internal
	// current position indicator, so just delegate actual processing to
	// pread, and update internal state accordingly.

	// Perform the read from the current file position.
	int rc = pread(buffer, size, _cur_pos);

	// Increment the current file position by the number of bytes that was read.
	// The number of bytes actually read may be less than 'size', so it's important
	// we only advance the current position by the actual number of bytes read.
	_cur_pos += rc;

	// Return the number of bytes read.
	return rc;
}

/**
 * Moves the current file pointer, based on the input arguments.
 * @param offset The offset to move the file pointer either 'to' or 'by', depending
 * on the value of type.
 * @param type The type of movement to make.  An absolute movement moves the
 * current file pointer directly to 'offset'.  A relative movement increments
 * the file pointer by 'offset' amount.
 */
void TarFSFile::seek(off_t offset, SeekType type)
{
	// If this is an absolute seek, then set the current file position
	// to the given offset (subject to the file size).  There should
	// probably be a way to return an error if the offset was out of bounds.
	if (type == File::SeekAbsolute) {
		_cur_pos = offset;
	} else if (type == File::SeekRelative) {
		_cur_pos += offset;
	}
	if (_cur_pos >= size()) {
		_cur_pos = size() - 1;
	}
}

TarFSNode::TarFSNode(TarFSNode *parent, const String& name, TarFS& owner) : PFSNode(parent, owner), _name(name), _size(0), _has_block_offset(false), _block_offset(0)
{
}

TarFSNode::~TarFSNode()
{
}

/**
 * Opens this node for file operations.
 * @return 
 */
File* TarFSNode::open()
{
	// This is only a file if it has been associated with a block offset.
	if (!_has_block_offset) {
		return NULL;
	}

	// Create a new file object, with a header from this node's block offset.
	return new TarFSFile((TarFS&) owner(), _block_offset);
}

/**
 * Opens this node for directory operations.
 * @return 
 */
Directory* TarFSNode::opendir()
{
	return new TarFSDirectory(*this);
}

/**
 * Attempts to retrieve a child node of the given name.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::get_child(const String& name)
{
	TarFSNode *child;

	// Try to find the given child node in the children map, and return
	// NULL if it wasn't found.
	if (!_children.try_get_value(name.get_hash(), child)) {
		return NULL;
	}

	return child;
}

/**
 * Creates a subdirectory in this node.  This is a read-only file-system,
 * and so this routine does not need to be implemented.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::mkdir(const String& name)
{
	// DO NOT IMPLEMENT
	return NULL;
}

/**
 * A helper routine that updates this node with the offset of the block
 * that contains the header of the file that this node represents.
 * @param offset The block offset that corresponds to this node.
 */
void TarFSNode::set_block_offset(unsigned int offset)
{
	_has_block_offset = true;
	_block_offset = offset;
}

/**
 * A helper routine that adds a child node to the internal children
 * map of this node.
 * @param name The name of the child node.
 * @param child The actual child node.
 */
void TarFSNode::add_child(const String& name, TarFSNode *child)
{
	_children.add(name.get_hash(), child);
}

TarFSDirectory::TarFSDirectory(TarFSNode& node) : _entries(NULL), _nr_entries(0), _cur_entry(0)
{
	_nr_entries = node.children().count();
	_entries = new DirectoryEntry[_nr_entries];

	int i = 0;
	for (const auto& child : node.children()) {
		_entries[i].name = child.value->name();
		_entries[i++].size = child.value->size();
	}
}

TarFSDirectory::~TarFSDirectory()
{
	delete _entries;
}

bool TarFSDirectory::read_entry(infos::fs::DirectoryEntry& entry)
{
	if (_cur_entry < _nr_entries) {
		entry = _entries[_cur_entry++];
		return true;
	} else {
		return false;
	}
}

void TarFSDirectory::close()
{

}

static Filesystem *tarfs_create(VirtualFilesystem& vfs, Device *dev)
{
	if (!dev->device_class().is(BlockDevice::BlockDeviceClass)) return NULL;
	return new TarFS((BlockDevice &) * dev);
}

RegisterFilesystem(tarfs, tarfs_create);
