/* file.c: Implementation of memory backed file object (mmaped object). */
#include "vm/vm.h"
#include "filesys/file.h"
#include "lib/round.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <string.h>
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

struct lazy_load_arg
{
	struct file *file;
	off_t ofs;
	int length;
	uint32_t read_bytes;
	uint32_t zero_bytes;
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

	if (page == NULL)
		return false;

	struct lazy_load_arg *aux = (struct lazy_load_arg *)page->uninit.aux;

	struct file *file = aux->file;
	off_t offset = aux->ofs;
	size_t page_read_bytes = aux->read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	file_seek(file, offset);

	if (file_read(file, kva, page_read_bytes) != (int)page_read_bytes)
	{
		// palloc_free_page (kva);
		return false;
	}

	memset(kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
// TODO - 구현,,
file_backed_swap_out(struct page *page)
{
	// 예외처리를 잘 해주자.
	if (!page)
	{
		return false;
	}
	struct file_page *file_page UNUSED = &page->file;
	void *addr = page->va;
	struct thread *t = thread_current();
	if (pml4_is_dirty(t->pml4, addr))
	{
		struct lazy_load_arg *file = file_page->file;
		size_t length = file_page->length;
		off_t offset = file_page->ofs;
		void *kva = page->frame->kva;
		if (file_write_at(file, kva, length, offset) != length)
		{
			return false;
		}
	}
	pml4_clear_page(t->pml4, addr);
	page->frame->page = NULL;
	page->frame = NULL;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page UNUSED = &page->file;

	file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
}

/* Do the mmap */

static bool
lazy_Fileload_segment(struct page *page, void *aux)
{
	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)aux;

	// 1) 파일의 position을 ofs으로 지정한다.
	// 2) 파일을 read_bytes만큼 물리 프레임에 읽어 들인다.
	file_read_at(lazy_load_arg->file, page->frame->kva, lazy_load_arg->read_bytes, lazy_load_arg->ofs);
	// 3) 다 읽은 지점부터 zero_bytes만큼 0으로 채운다.
	memset(page->frame->kva + lazy_load_arg->read_bytes, 0, lazy_load_arg->zero_bytes);

	// struct file *f = page->file.file;
	page->file.file = lazy_load_arg->file;
	page->file.read_bytes = lazy_load_arg->read_bytes;
	page->file.ofs = lazy_load_arg->ofs;
	page->file.length = lazy_load_arg->length;
	// 파일에다 저장하는 이유 = 디스크 멥핑 해재할때, 다시 롸이트할떄 사용하기 위해,,
	//  디스크에서 메모리로 연결시킬떄 저장시켜놧던거를...그....와일문 돌면서 햇던것을 여기서 해줘야 나중에 쓸 수 있음.

	return true;
}

void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
// 파일 타입이 file인 uninit 페이지를 생성하고,  이후 페이지 폴트가 발생하면 해당 페이지는 file 타입의 페이지로 초기화 되며 물리 프레임과 연결됨.

{
	// 그사이에 파일이 close 가 되었을 경우를 대비하기 위함.

	struct file *f = file_reopen(file);
	// 파일의 오프셋이 넘어오니까 거기부터 mmap 을 하겠다는건데
	//  file_seek로 해서 거기서 오프셋으로 넘기면 됨
	// file_seek(f, offset);

	// 파일이 여러페이지를 넘어가서 쓸수 있는 경우를 대비하기 위함.
	// 순전히 이 이유라기 보단, read bytes = 총 읽어야되는 파일 길이
	// zero_bytes : 페이지 얼라인드 하기 위함
	// 초기값 설정
	// 인자 길이 = 받아야할 메모리,

	// size_t zero_bytes = PGSIZE - read_bytes;
	// ASSERT((read_bytes + zero_bytes) % PGSIZE == 0); // read_bytes + zero_bytes 페이지가 PGSIZE의 배수인가?
	// ASSERT(pg_ofs(addr) == 0); // ofs가 페이지 정렬 되어있는지 확인
	// page주소값을 가지고있음, 만약 여러페이지를 쓰면 pgsize 만큼 더해가며 업데이트 해나가는 값.
	void *upage = addr;
	int count = length % PGSIZE != 0 ? (int)(length / PGSIZE) + 1 : (int)(length / PGSIZE);
	// printf("%d", length);
	while (length > 0) // read_byte, zero_bytes가 0보다 클때 동안 루프
	{
		size_t page_read_bytes = PGSIZE < length ? PGSIZE : length;
		// pgsize 만큼 파일을 읽으면서..
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		// size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE; // 최대로 읽을 수 있는 크기는 PGSIZE
		// size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// 페이지에 내용을 로드할때 사용할 함수와 필요한 인자 넣어주기.
		//  vm_alloc_page_with initializer 의 4,5 번쨰 인자가 로드할때 사용할 함수, 필요한 인자.
		// size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// 변경할 필요없음.
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));

		lazy_load_arg->file = f;					 // 내용이 담긴 파일 객체
		lazy_load_arg->ofs = offset;				 // 이 페이지에서 읽기 시작할 위치
		lazy_load_arg->length = count;				 // 490
		lazy_load_arg->read_bytes = page_read_bytes; // 이 페이지에서 읽어야 하는 바이트 수
		lazy_load_arg->zero_bytes = page_zero_bytes; // 이 페이지에서 read_bytes만큼 읽고 공간이 남아 0으로 채워야 하는 바이트 수
													 // vm_alloc_page_with_initializer를 호출하여  대기 중인 객체를 생성합니다.

		// TODO - lazy_fileload_segment 함수 만드는게 핵심임.
		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_Fileload_segment, lazy_load_arg))
		{
			free(lazy_load_arg);
			return false;
		}

		/* Advance. */
		// 다음 반복을 위하여 읽어들인 만큼 값을 갱신합니다.
		// read_bytes -= page_read_bytes;
		// zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		offset += page_read_bytes;
		length -= page_read_bytes;
	}
	// printf("안녕하지못해\n");
	return upage;
}

/* Do the munmap */
// TODO - mmap()함수의 역연산을 하는 함수.
void do_munmap(void *addr)
{
	//  물리 프레임과의 연결을 끊어주어야 함. 다만,,수정사항이 있을 경우 이를 감지하여 변경사항을 디스크파일에 써줘야함
	//  변경이 되어있을 경우, 디스크에 존재하는 file에 write 해주고,
	//  dirty-beat를 다시 0으로 변경시켜 줌.
	//  length = read_bytes는 페이지안에서 읽은것, 렝스는 그냥 전체..

	// 총 전체ㅐ 길이->read_bytes랑 무엇이 다른가?
	struct thread *cur_t = thread_current();
	struct page *p = spt_find_page(&cur_t->spt, addr);
	struct file *file = p->file.file;
	int total_length = p->file.length;

	while (total_length > 0)
	{
		p = spt_find_page(&cur_t->spt, addr);

		if (p == NULL)
			return NULL;
		// struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)p->uninit.aux;
		// file-uninitializer에서 aux

		if (pml4_is_dirty(cur_t->pml4, addr))
		{
			file_write_at(file, addr, p->file.read_bytes, p->file.ofs);
		}
		//  변경이 되어있지 않을 경우 해당 페이지를 pml4에서 삭제해주고, addr을 다음 페이지 주소로 변경하기

		pml4_clear_page(cur_t->pml4, addr);
		// TODO- 인자넣기; 		p가 아닌 이유,, 프레임의 크바, 물리메모리의 페이지를 프리해야 하므로,,

		if (p->frame)
		{
			palloc_free_page(p->frame->kva);
		}
		hash_delete(&cur_t->spt.spt_hash, &p->hash_elem);
		free(p);
		addr += PGSIZE;

		// total_length -= p->file.read_bytes;
		total_length--;
	}
	file_close(file);
}
