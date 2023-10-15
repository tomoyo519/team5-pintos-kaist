/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "kernel/hash.h"
#include "threads/thread.h"
#include "lib/string.h"


#include <stdio.h>
static struct list frame_table;
static struct list_elem *start;
bool hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux);
/* 가상 메모리 서브시스템을 초기화하기 위해
 * 각 서브시스템의 초기화 코드를 호출합니다. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	list_init(&frame_table);           // 수정!
	start = list_begin(&frame_table);
}

/* 페이지 유형을 가져옵니다.
 * 이 함수는 페이지가 초기화된 후의 유형을 알고 싶을 때 유용합니다.
 * 이 함수는 현재 완전히 구현되었습니다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer와 함께 대기 중인(pending) 페이지 객체를 생성합니다.
 * 페이지를 만들려면 직접 만들지 말고
 * 이 함수나 `vm_alloc_page`를 통해 만드세요. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	/* Check wheter the upage is already occupied or not. */
	// struct page *new_page;
	//  vm_initializer *init;
	if (spt_find_page (spt, upage) == NULL) {
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

		/* TODO: 페이지를 생성하고 VM 유형에 따라 초기화자를 가져온 다음
		 * uninit_new를 호출하여 "uninit" 페이지 구조체를 만드십시오.
		 * uninit_new를 호출한 후 필드를 수정해야 합니다. */
		
		struct page *new_page = (struct page *)calloc(1, sizeof(struct page));

		uninit_new (new_page, upage, init, type, aux, page_init);
		new_page->writable = writable;

		/* TODO: 페이지를 spt에 삽입하십시오. */
		return spt_insert_page(spt, new_page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
  struct page *page = NULL;
  /* TODO: Fill this function. */
  void *page_addr = pg_round_down(va);

  struct page pg;
  pg.va = page_addr;
  struct hash_elem *found = hash_find(&(spt->hash), &(pg.h_elem));
  if (found == NULL)
    return NULL;
  page = hash_entry(found, struct page, h_elem);

  return page;

}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {

	if(page == NULL){
		return false;
	}
	return hash_insert(&spt->hash, &page->h_elem) == NULL ? true : false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// swap_out();
	// swap_out()
	// return NULL;
}

/* palloc() 및 프레임 가져오기.
 * 사용 가능한 페이지가 없는 경우 페이지를 제거하고 반환합니다.
 * 항상 유효한 주소를 반환합니다. 
 * 사용자 풀 메모리가 가득 찬 경우,
 * 이 함수는 사용 가능한 메모리 공간을 얻기 위해 프레임을 제거합니다. */
static struct frame *
vm_get_frame (void) {
	// struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
  struct frame *frame = NULL;
  /* TODO: Fill this function. */
  void *pg_ptr = palloc_get_page(PAL_USER);
  if (pg_ptr == NULL)
  {
    return vm_evict_frame();
  }

  frame = (struct frame *)malloc(sizeof(struct frame));
  frame->kva = pg_ptr;
  frame->page = NULL;

  ASSERT(frame != NULL);
  ASSERT(frame->page == NULL);
  return frame;

}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	struct page *page = spt_find_page(spt, addr);
	if(page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA.
 spt를 통해 페이지를 찾아줌
 */
bool
vm_claim_page (void *va ) {

	struct page * page = spt_find_page(&thread_current()->spt, va);
	if(page == NULL){
		return false;
	}
	// printf("@@ stack page found: %p\n", page->va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	ASSERT(frame && frame->kva)
	/* Set links */

	frame->page = page;
	page->frame = frame;

	if(pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)){
		
		return swap_in (page, frame->kva);
	}
	return false;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
}
// # include "mmu.h"
bool
hash_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct page * p_a = hash_entry(a, struct page, h_elem);
	struct page * p_b = hash_entry(b, struct page, h_elem);
	return p_a->va < p_b->va;
}

uint64_t
do_hashing(const struct hash_elem *a, void *aux){
	struct page * p_a = hash_entry(a, struct page, h_elem);
	return hash_bytes(&p_a->va, sizeof p_a->va);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash, do_hashing, hash_compare, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
