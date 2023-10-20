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

	struct file_page *file_page = &page->file;
	//memcpy
	// page->file = *file_page;
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
	struct file_page *file_page = &page->file;
	file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
}

static bool
lazy_load_file (struct page *page, void *aux) {

	struct aux_file_info *f_info = (struct aux_file_info *)aux;

	struct file *file = f_info->file;
	size_t page_read_bytes = f_info->read_bytes;
	size_t page_zero_bytes = f_info->zero_bytes;
	off_t ofs = f_info->ofs;

	// file_seek (file, ofs);
	file_read_at(file, page->frame->kva,page_read_bytes, ofs);
	// file_read (file, page->frame->kva, page_read_bytes);
	memset (page->frame->kva + page_read_bytes, 0, page_zero_bytes);

	page->file.file = file;
	page->file.read_bytes = page_read_bytes;
	page->file.ofs = ofs;
	page->file.pg_cnt = f_info->pg_cnt;
	// printf("\n\n  @@@@ page_read_bytes:%d , zero_bytes:%d, offset:%d \n\n", page_read_bytes,page_zero_bytes,ofs  );
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	// 외부에서 file 을 close 할 경우 대비
	struct file *reopen_file = file_reopen(file);

	ASSERT (offset % PGSIZE == 0);

	void * address = addr;

	// 할당받아야 할 페이지 개수
	int pg_cnt = length % PGSIZE != 0 ? (int)(length/PGSIZE) + 1 : (int)(length/PGSIZE);

	while(length > 0) {
		size_t page_read_bytes = length < PGSIZE ? length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct aux_file_info *f_info = (struct aux_file_info *)calloc(1, sizeof(struct aux_file_info));
		f_info->file = reopen_file;
		f_info->read_bytes = page_read_bytes;
		f_info->zero_bytes = page_zero_bytes;
		f_info->pg_cnt = pg_cnt;
		f_info->ofs = offset;
		f_info->writable = writable;
		
		if (!vm_alloc_page_with_initializer (VM_FILE, addr, writable, lazy_load_file, f_info)){
			free(f_info);
			return false;
		}

		/* Advance. */
		length -= page_read_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;

	}
	return address;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread * curr = thread_current();
	
	struct page *page;
	
	page = spt_find_page(&curr->spt, addr);

	struct file_page *f_page = &page->file;

	struct file * file = page->file.file;
	
	// 페이지 찾기
	int pg_cnt = page->file.pg_cnt;
	printf("\n @@@ page_cnt :::: %d\n\n", pg_cnt);
	// printf("\n\n  @@@@ page_read_bytes:%d , pg_cnt:%d, offset:%d \n\n", f_page->read_bytes,f_page->pg_cnt,f_page->ofs  );
	// printf(" @@@ 페이지 수 : %d  , @@@ 주소 : %p\n", pg_cnt, addr);

	while(pg_cnt > 0){
		page = spt_find_page(&curr->spt, addr);

		if(page == NULL){
			// printf("NO PAGE !!\n");
			break;
		}

		f_page = &page->file;
		size_t read_bytes = f_page->read_bytes;
		off_t ofs = f_page->ofs;
		// printf("\n---- (read_bytes : %d) , (ofs : %d)\n", read_bytes, ofs);
		// printf("\n---- (addr : %p) , (pg_cnt : %d)\n", addr, pg_cnt);
		// printf("\n---- 내용쓰~~~~~ \n %s)\n", page->va);

		// 페이지가 수정되었는지 확인
		if(pml4_is_dirty(curr->pml4, addr) == 1){

			// 디스크에 파일 쓰기
			file_write_at(file, addr, read_bytes, ofs);

			// dirty-beat 를 다시 0으로 변경 (초기화)
			pml4_set_dirty(curr->pml4, addr, false);

		}
		// 페이지 수정 안되있다면 삭제
		// 유저/커널 가상주소 연결 끊음!

		pg_cnt--;
		pml4_clear_page(curr->pml4, addr);
		addr += PGSIZE;
		palloc_free_page(page->frame->kva);
		hash_delete(&curr->spt.hash, &page->h_elem);
		// printf("unmmap 1번 완료!\n");

		// spt_remove_page(&curr->spt, page);
		// printf(" $$$$$$ \n");

		
		
		// page = spt_find_page(&curr->spt, addr);
		// if(page == NULL) break;
		// printf("  @@ page?? %p \n ", addr);
		// free();
		// printf(" @@@ 페이지 줄어든다~~ : %d  , @@@ 주소는 늘어난다~~ : %p\n", page_len, addr);
	};
	file_close(file);

}
