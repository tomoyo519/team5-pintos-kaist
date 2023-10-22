/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

#define SECTORS_IN_PAGE 8 // sector * 8 = slot = 1 page
struct bitmap *swap_table;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	// 1:1 - 스왑(교환) 영역
	swap_disk = disk_get(1,1);

	/* sectors / 8 = slot index */
	size_t bitcnt = disk_size(swap_disk)/SECTORS_IN_PAGE;
	swap_table = bitmap_create(bitcnt);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
		size_t sec_no = anon_page->swap_index;

	//  sec_no 그룹에 비트가 할당되어 있지 않다면 return (디스크에 write 되지 않았음)
    if (bitmap_test(swap_table, sec_no) == false) 
        return false;


    for (int i = 0; i < SECTORS_IN_PAGE; ++i) {
        disk_read(swap_disk, sec_no * SECTORS_IN_PAGE + i, kva + DISK_SECTOR_SIZE * i);
    }

    bitmap_set(swap_table, sec_no, false);
    
    return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// page = evict page 에 대한 정보를 swap disk 에 저장
	// Swap out : 유저 프로그램이 메모리 요청했지만, 다 차 있기 때문에 일부 페이지를 퇴출한다. 퇴출된 페이지는 메모리에서 삭제되고 디스크에 복사된다.

	/* 1. 페이지를 할당받을 수 있는 swap slot 하나를 찾기 */
	//	  Swap table 에서 0번째부터 false값을 갖는 비트를 1개 찾기 (false=할당가능)
	size_t sec_no = bitmap_scan(swap_table, 0, 1, false);

	if(sec_no == BITMAP_ERROR)
		return false;

	/* 2. 디스크에 페이지를 복사하기 */
	//    sector(512 bytes) * 8 = slot = 1 page
	// SEC_NO를 버퍼로 디스크에 기록 - 버퍼는 512 바이트를 포함해야 합니다
	for(int i = 0; i < SECTORS_IN_PAGE; i++){
		disk_write(swap_disk, sec_no * SECTORS_IN_PAGE + i
							, page->va + DISK_SECTOR_SIZE * i);
	}

	/* 3. 페이지 댜헌 swap slot의 비트를 true(할당됨)로 바꿔주기 */
    //    해당 페이지의 PTE에서 present bit을 0으로 바꿔준다.
    //    이제 프로세스가 이 페이지에 접근하면 page fault가 뜬다.    
    bitmap_set(swap_table, sec_no, true);
    pml4_clear_page(thread_current()->pml4, page->va);

    /* 페이지의 swap_index 값을 이 페이지가 저장된 swap slot의 번호로 써준다.*/
    anon_page->swap_index = sec_no;
    return true;
	
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
