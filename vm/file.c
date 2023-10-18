/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "filesys/file.h"
#include "lib/round.h"
#include "vaddr.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */

struct lazy_load_arg
{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
// {
// 	// TODO -
// 	/*
// 	우선적으로 인자로 들어온 file을 reopen()을 통해 동일한 파일에 대해 다른 주소를 가지는 파일 구조체를 생성합니다. reopen()하는 이유는 mmap을 하는 동안 만약 외부에서 해당 파일을 close()하는 불상사를 예외처리하기 위함입니다.
// 저희 팀의 경우 해당 함수를 구현할때 prcess.c의 load_segment()함수를 떠올렸습니다.생각해보면 load_segment() 또한 load()에서 open()한 파일을 받아서 lazy-loading을 수행하도록 페이지를 생성해줍니다.즉,사실상 거의 동일한 로직을 사용해도 된다는 것입니다.
// 다만 차이점이 한가지 있다면 load_segment()는 페이지 이니셜라이저를 호출할때 페이지의 타입의 ANON으로 설정했고 do_mmap()은 이와 달리 FILE 타입으로 인자를 넘겨줘야한다는것입니다.
// 구현적으로 팁을 드리자면 해당 함수는 void*형의 주소를 반환해야합니다.즉,성공적으로 페이지를 생성하면 addr을 반환합니다.하지만 addr 주소의 경우 iter를 돌면서 PGSIZE만큼 변경되기 때문에 초기의 addr을 리턴값으로 저장해두고 iter를 돌아야합니다.
// 	*/
// 	// reopen 하는 이유 => 외부에서 해당 파일을 cloase 하는 불상사를 처리하기 위함.

//
// }
// todo : file 을
{
	// 그사이에 파일이 close 가 되었을 경우를 대비하기 위함.
	struct file *f = file_reopen(file);
	// 파일의 오프셋이 넘어오니까 거기부터 mmap 을 하겠다는건데
	//  file_seek로 해서 거기서 오프셋으로 넘기면 됨
	file_seek(f, offset);

	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0); // read_bytes + zero_bytes 페이지가 PGSIZE의 배수인가?
	ASSERT(pg_ofs(upage) == 0);						 // upage 페이지 정렬되어있는지 확인
	ASSERT(ofs % PGSIZE == 0);						 // ofs가 페이지 정렬 되어있는지 확인
	// 파일이 여러페이지를 넘어가서 쓸수 있는 경우를 대비하기 위함.
	// 순전히 이 이유라기 보단, read bytes = 총 읽어야되는 파일 길이
	// zero_bytes : 페이지 얼라인드 하기 위함
	// 초기값 설정
	read_bytes = file_length(f);
	zero_bytes = ROUND_UP(read_bytes, PGSIZE) - read_bytes;
	off_t ofs = offset;
	// page주소값을 가지고있음, 만약 여러페이지를 쓰면 pgsize 만큼 더해가며 업데이트 해나가는 값.
	uint8_t *upage = addr;
	while (read_bytes > 0 || zero_bytes > 0) // read_byte, zero_bytes가 0보다 클때 동안 루프
	{
		// pgsize 만큼 파일을 읽으면서..

		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE; // 최대로 읽을 수 있는 크기는 PGSIZE
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// 페이지에 내용을 로드할때 사용할 함수와 필요한 인자 넣어주기.
		//  vm_alloc_page_with initializer 의 4,5 번쨰 인자가 로드할때 사용할 함수, 필요한 인자.

		// 변경할 필요없음.
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
		lazy_load_arg->file = file;					 // 내용이 담긴 파일 객체
		lazy_load_arg->ofs = ofs;					 // 이 페이지에서 읽기 시작할 위치
		lazy_load_arg->read_bytes = page_read_bytes; // 이 페이지에서 읽어야 하는 바이트 수
		lazy_load_arg->zero_bytes = page_zero_bytes; // 이 페이지에서 read_bytes만큼 읽고 공간이 남아 0으로 채워야 하는 바이트 수
		// vm_alloc_page_with_initializer를 호출하여 대기 중인 객체를 생성합니다.

		// TODO - lazy_fileload_segment 함수 만드는게 핵심임.
		if (!vm_alloc_page_with_initializer(VM_FILE, upage,
											writable, lazy_Fileload_segment, lazy_load_arg))
			return NULL;

		/* Advance. */
		// 다음 반복을 위하여 읽어들인 만큼 값을 갱신합니다.
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	// mmap 을 시작하는 주소
	return addr;
}

// TODO - 몇줄만 바꾸면 됨.. 뭘바꾸는데 ;_;
static bool
lazy_Fileload_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)aux;

	// 1) 파일의 position을 ofs으로 지정한다.
	file_seek(lazy_load_arg->file, lazy_load_arg->ofs);
	// 2) 파일을 read_bytes만큼 물리 프레임에 읽어 들인다.
	if (file_read(lazy_load_arg->file, page->frame->kva, lazy_load_arg->read_bytes) != (int)(lazy_load_arg->read_bytes))
	{
		palloc_free_page(page->frame->kva);
		return false;
	}
	// 3) 다 읽은 지점부터 zero_bytes만큼 0으로 채운다.
	memset(page->frame->kva + lazy_load_arg->read_bytes, 0, lazy_load_arg->zero_bytes);

	return true;
}

/* Do the munmap */
void do_munmap(void *addr)
{
	// TODO -
}
