/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <string.h>
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

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
	
	struct uninit_page *uninit = &page->uninit;
	
	struct aux_file_info *f_info = (struct aux_file_info*)uninit->aux;
	page->file.file = f_info->file;
	page->file.read_bytes = f_info->read_bytes;
	page->file.pg_cnt = f_info->pg_cnt;
	page->file.ofs = f_info->ofs;


	return true;
}


/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	/*파일에서 콘텐츠를 읽어 kva 페이지에서 swap in
	  파일 시스템과 동기화하기*/
	struct file *file = file_page->file;
	size_t read_bytes = file_page->read_bytes;
	off_t ofs = file_page->ofs;
	int pg_cnt = file_page->pg_cnt;
	

	file_read_at(file, kva, read_bytes, ofs);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	uint64_t curr_pml4 = thread_current()->pml4;
	
	struct file *file = file_page->file;
	size_t read_bytes = file_page->read_bytes;
	off_t ofs = file_page->ofs;
	
	// dirty 한지 확인
	if(pml4_is_dirty(curr_pml4, page->va)){

		file_write_at(file, page->frame->kva, read_bytes, ofs);
		pml4_set_dirty(curr_pml4, page->va, 0);
	}

	// page-frame 연결 끊기
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(curr_pml4, page->va);
	return true;

}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
	// pml4_clear_page()
}

static bool
lazy_load_file (struct page *page, void *aux) {

	struct aux_file_info *f_info = (struct aux_file_info *)aux;

	struct file *file = f_info->file;
	size_t page_read_bytes = f_info->read_bytes;
	size_t page_zero_bytes = f_info->zero_bytes;
	off_t ofs = f_info->ofs;

	file_read_at(file, page->frame->kva,page_read_bytes, ofs);
	memset (page->frame->kva + page_read_bytes, 0, page_zero_bytes);

	/* 디스크에서 읽어온 파일 기반 페이지 */
	page->file.file = file;
	page->file.read_bytes = page_read_bytes;
	page->file.ofs = ofs;
	page->file.pg_cnt = f_info->pg_cnt;
	// free(f_info);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	// 외부에서 file 을 close 할 경우 대비
	struct file *reopen_file = file_reopen(file);

	void * origin_addr = addr;

	// 할당받아야 할 페이지 개수
	int pg_cnt = length % PGSIZE != 0 ? (int)(length/PGSIZE) + 1 : (int)(length/PGSIZE);
	// length : 읽어야할 길이 = 할당받아야할 메모리
	while(length > 0) {
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct aux_file_info *f_info = (struct aux_file_info *)calloc(1, sizeof(struct aux_file_info));
		f_info->file = reopen_file;
		f_info->read_bytes = page_read_bytes;
		f_info->zero_bytes = page_zero_bytes;
		f_info->pg_cnt = pg_cnt;
		f_info->ofs = offset;
		
		// 파일을 기반으로 한 페이지 또한 lazy 하게 물리메모리에 load
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_file, f_info)){
			free(f_info);
			return false;
		}

		length -= page_read_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
	}
	return origin_addr;
}

void
do_munmap (void *addr) {
	struct thread * curr = thread_current();
	
	struct page *page;
	page = spt_find_page(&curr->spt, addr);

	struct file * file = page->file.file;
	
	// 페이지 찾기
	int pg_cnt = page->file.pg_cnt;
	while(pg_cnt > 0){
		page = spt_find_page(&curr->spt, addr);

		if(page == NULL)
			break;

		// 페이지가 수정되었는지 확인
		if(pml4_is_dirty(curr->pml4, addr) == 1){

			// 디스크에 파일 쓰기
			file_write_at(file, addr, page->file.read_bytes, page->file.ofs);

			// dirty-beat 를 다시 0으로 변경 (초기화)
			pml4_set_dirty(curr->pml4, addr, false);

		}
		// 유저-커널 가상주소 연결 끊음!
		pml4_clear_page(curr->pml4, addr);

		// swap out 했을 때 page->frame = NULL 처리 때문에 조건문 필요
		if(page->frame){
			palloc_free_page(page->frame->kva);
		}
		hash_delete(&curr->spt.hash, &page->h_elem);
		addr += PGSIZE;
		pg_cnt--;
	};
	file_close(file);

}
