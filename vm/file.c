/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "userprog/syscall.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
bool vm_alloc_page_with_initializer_ (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux);
static bool load_segment_ (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		int fd, off_t offset) {
	if (fd == NULL || fd == 0 || fd == 1|| filesize(fd) == 0 || addr == 0 || pg_no(addr) == 0 || pg_no(addr) == 1){
		return NULL;
	}
	struct file * file = process_get_file(fd);
	
	load_segment_(file, offset, addr, length-(PGSIZE - length%PGSIZE)%PGSIZE, (PGSIZE - length%PGSIZE)%PGSIZE ,true);
	
	return addr;
}
	
	

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *th_cur = thread_current(); 
	struct page * page = spt_find_page(&th_cur->spt,addr);
	struct file * this_file = page->being_backed;
	uint32_t read_bytes = page->ready_bytes;
	file_seek(this_file, page->offset);
	for (addr;read_bytes !=0 && (page = spt_find_page(&th_cur->spt,addr))->being_backed == this_file;addr = addr+PGSIZE){
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		if (pml4_is_dirty(th_cur->pml4, addr)){
			file_write(this_file,addr,page_read_bytes);
		}
		file_seek(this_file, this_file->pos+page_read_bytes);
		read_bytes -= page_read_bytes;
		pml4_clear_page(&th_cur->spt, addr);
		
		
	}
}


bool
vm_alloc_page_with_initializer_ (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	struct page * old_page = spt_find_page (spt, upage);

	/* Check wheter the upage is already occupied or not. */
	/* if occupied, then it must not be VM_FILE and writable should be true. */
	if (old_page == NULL || (old_page->writable == true && old_page->operations->type !=VM_FILE)) {
		// initializer
		initializer *page_init = NULL;

		switch (type)
		{
		case VM_ANON:
			page_init = anon_initializer;
			break;
		case VM_FILE:
			page_init = file_backed_initializer;
		default:
			break;
		}

		struct page *new_page = (struct page *)calloc(1, sizeof(struct page));
		uninit_new (new_page, upage, init, type, aux, page_init);
		new_page->writable = writable;
		new_page->being_backed = ((struct file_info *)aux)->file;
		// new_page->ready_bytes = ((struct file_info *)aux)->read_bytes;
		new_page->offset = ((struct file_info *)aux)->ofs;
		return spt_insert_page(spt, new_page);
	}

err:
	return false;
}


static bool
load_segment_ (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	// ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	// ASSERT (pg_ofs (upage) == 0);
	// ASSERT (ofs % PGSIZE == 0);

	if (((read_bytes + zero_bytes) % PGSIZE != 0) || (pg_ofs (upage) != 0) || (ofs % PGSIZE != 0)){
		return false;
	}
	
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct file_info *f_info = (struct file_info *)calloc(1, sizeof(struct file_info));
		f_info->file = file;
		f_info->read_bytes = page_read_bytes;
		f_info->zero_bytes = page_zero_bytes;
		f_info->ofs = ofs;

		if (!vm_alloc_page_with_initializer_ (VM_FILE, upage,
					writable, lazy_load_segment, f_info))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	return true;
}




// static bool
// unload_segment_ (struct file *file, off_t ofs, uint8_t *upage,
// 		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

// 	if (((read_bytes + zero_bytes) % PGSIZE == 0) || (pg_ofs (upage) == 0) || (ofs % PGSIZE == 0)){
// 		return false;
// 	}
	
// 	while (read_bytes > 0 ) {
// 		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;


// 	//write at 해야지 / ofs는 어디에 쓰여??/

// 		/* Advance. */
// 		read_bytes -= page_read_bytes;
// 		upage += PGSIZE;
// 		ofs += page_read_bytes;
// 	}
// 	return true;
// }