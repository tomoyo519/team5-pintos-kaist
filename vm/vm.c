/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"

struct lazy_load_arg
{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};

static struct list frame_list;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	list_init(&frame_list);
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
// pending 중인 페이지 객체를 초기화하고 생성합니다.
// 페이지를 생성하려면 직접 생성하지 말고 이 함수나 vm_alloc_page를 통해 만드세요.
// init과 aux는 첫 page fault가 발생할 때 호출된다.
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	// upage가 이미 사용 중인지 확인합니다.
	if (spt_find_page(spt, upage) == NULL)
	{
		/* : Create the page, fetch the initialier according to the VM type,
		 * : and then create "uninit" page struct by calling uninit_new. You
		 * : should modify the field after calling the uninit_new. */
		// 페이지를 생성하고,
		struct page *p = (struct page *)malloc(sizeof(struct page));
		// VM 유형에 따라 초기화 함수를 가져와서
		//// 함수 포인터방식.. 함수 자체가 주소값을 가지고있기 때문에 이름만 넘기면 된다.
		bool (*page_initializer)(struct page *, enum vm_type, void *);

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		}
		// uninit_new를 호출해 "uninit" 페이지 구조체를 생성하세요.
		uninit_new(p, upage, init, type, aux, page_initializer);
		// uninit_new를 호출한 후에는 필드를 수정해야 합니다.
		p->writable = writable;

		/* : Insert the page into the spt. */
		// printf("여기까지 와?\n");
		return spt_insert_page(spt, p);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// spt에서 va에 해당하는 page를 찾아서 반환
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{
	struct page *page = NULL;
	page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;

	// va에 해당하는 hash_elem 찾기
	page->va = pg_round_down(va); // page의 시작 주소 할당
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);

	// 있으면 e에 해당하는 페이지 반환
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{

	return hash_insert(&spt->spt_hash, &page->hash_elem) == NULL ? true : false; // 존재하지 않을 경우에만 삽입
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = list_entry(list_pop_front(&frame_list), struct frame, frame_elem);

	// 엘렘과 쓰레드 커렌트 설정
	// 반복문 돌면서, pml4 _is_accessed true 이면, pml4_set_accessed
	// 아니라면 vicitm 리턴해주기.
	// 반복문을 두 번 돌리는 이유?  첫번째 반복문에서 찾지 못한경우, 두번쨰 반복문에서 찾기 위해서..

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
// 페이지를 배신..하고 다른데 가서 달라붙음.
//  palloc 해서 NULL 이 나오는 경우, 다른 프레임 떼와서 붙여주기.
//  전\체프레임 frame list, elem 으로 연결관리
//  swap table 은 anony에만 필요.
//  file은
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	// NOTE - 페이지가, file-backed냐, anon이냐에 따라서 호출되는 하ㅏㅁ수가 달라짐.
	// anonymous 인 경우, 디스크에[ backing store가 따로 없기 때문에 만들어 줘야 함.
	swap_out(victim->page);
	memset(victim->kva, 0, PGSIZE);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
// vm_try_handle_fault 에서, vm_clain_page가 불리는 경우는 스택을 늘려줘서 해결되는 것이 아닌 다른 경우
// 의 fault 가 난 주소에 대해서, 페이지와 물리 프레임을 연결시켜주기 위해서
// 이떄 물리 프레임을 연결시켜주려면, 빈 프레임을 찾아야 하는데, 이를 담당하는 함수 = vm_get_frame()
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;

	void *kva = palloc_get_page(PAL_USER | PAL_ZERO); // user pool에서 새로운 physical page를 가져온다.

	// if (kva == NULL)		   // page 할당 실패 -> 나중에 swap_out 처리
	// 	swap_out(frame->page); // OS를 중지시키고, 소스 파일명, 라인 번호, 함수명 등의 정보와 함께 사용자 지정 메시지를 출력

	if (!kva)
	{

		frame = vm_evict_frame();

		list_push_back(&frame_list, &frame->frame_elem);
		return frame;
	}
	else
	{
		frame = (struct frame *)malloc(sizeof(struct frame)); // 프레임 할당
		frame->kva = kva;									  // 프레임 멤버 초기화
		frame->page = NULL;
		list_push_back(&frame_list, &frame->frame_elem);
	}
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);

	return frame;
	// palloc 으로 공간을 받오울 수가 없다면 => frame -> kva == NULL 이라면, evict를 통해 프레임을 차지하고있는 페이지를 쫓아내야 한다.
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	// printf("vm_stac-growth 해야햄\n");
	// 하나 이상의 anonymous 페이지를 할당하여 스택 크기를 늘립니다. 이로써 addr은 faulted 주소(폴트가 발생하는 주소) 에서 유효한 주소가 됩니다.
	// 페이지를 할당할 때는 주소를 PGSIZE 기준으로 내림하세요.
	// void *new_page = vm_get_frame();
	// if (new_page == NULL)
	// {
	// 	return false;
	// }
	// pml4_set_page(thread_current()->pml4, addr, new_page, 1);
	// // fault handler 에서 필요할때 호출하도록 수정하기.
	return vm_alloc_page(VM_ANON, addr, 1);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
// spt_find_page를 통해 SPT를 참조하여 Faulted address 에 해당하는 페이지 구조체를 해결하는 함수
// pagefulat 가 발새아면 제어권 받는 함수.
// 물리프레임이 존재하지않아서 발생한 예외일 경우 not present = true 를 전달받고, 이경우인 경우
// spt에서 해당 주소에 해당하는 페이지가 있는지 확인해서 존재한다면 해당 페이지에 물리 프레임 할당을 요청하는
// vm_do_claim_page 함수를 호출한다.
// not_present 가 faulse 인 경우, 물리 프레임이 할당 되어있지만,page fault 가 일어난것이므로
// read_only page에 write를 한 경우가 된다. 따라서 not_present가 false 인 경우는 예외로 처리..
// 인자 f = 페이지폴트 혹은 시스템 콜 발생시, 그 순간의 레지스터를 담고있는 구조체
// addr: 페이지 폴트 일으킨 가상 주소, user: 해당값이 true일때, 현재 쓰레드가 유저모드에서 돌아가다가 페이지 폴트를 일으킴. 현재 쓰레드의 rsp가 유저영역인지 커널영역인지
// write : true 일경우, 해당 해당 페이지 폴트가 쓰기요청이구, 그렇지 않은 경우 읽기요청
// not-present : false인 경우, readonly 페이지에 쓰기하려는 상황

// 이 함수는 페이지 폴트가 발생한 가상 주소 및 인자들이 유효한지 체크하고, stack growth 하
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	if (addr == NULL)
		return false;

	if (is_kernel_vaddr(addr))
		return false;

	// 접근하려는 주소가 현재 스택 포인터보다 아래 있고, 그 차이가 한 페이지 내라면, 스택증가.
	// printf("🥰%d %d \n", addr, f->rsp);
	// if (addr != f->rsp)
	if (addr == f->rsp && addr < USER_STACK && USER_STACK - (1 << 20) < addr)
	{
		// addr = rsp임.
		//  stack growth
		// printf("조건 드루와\n");
		vm_stack_growth(pg_round_down(addr));
		// return true;
	}

	if (not_present) // 접근한 메모리의 physical page가 존재하지 않은 경우
	{
		/* : Validate the fault */
		page = spt_find_page(spt, addr);
		if (page == NULL)
			return false;
		if (write == 1 && page->writable == 0) // write 불가능한 페이지에 write 요청한 경우
			return false;

		return vm_do_claim_page(page);
	}

	return false;
}
/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
// va로 page를 찾아서 vm_do_claim_page를 호출하는 함수
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;

	// spt에서 va에 해당하는 page 찾기
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
		return false;
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* : Insert page table entry to map page's VA to frame's PA. */
	// 가상 주소와 물리 주소를 매핑
	struct thread *current = thread_current();
	pml4_set_page(current->pml4, page->va, frame->kva, page->writable);
	// page_oprations 의 swap_in이 호출됨 -> uninit_initailze가 호출되면서 uninit페이지의 초기화가 이루어진다.
	/*
	static const struct page_oprations uninit_ops = {
		.swap_in = uninit_initialize,
		.swap_out = NULL,
		.destory = uninit_destory,
		.type = VM_UNINIT,
	}*/
	// 페이지가 실제로 로딩될때 = 첫번째 page fault 가 발생했을떄 호출되는 swap_in은
	// page_fault 에서 이어지는 vm_do_claim_page 함수에서 호출됨.
	return swap_in(page, frame->kva); // uninit_initialize
}

/* Returns a hash value for page p. */
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
// 부모프로세스가 가지고있는 본인의 spt 정보를 빠짐없이 자식 프로세스에게 복사해줌. fork 시스템콜
// spt iteration 해주기.
// iter 돌때마다 해당 hash_elem 과 연결된 page를 찾아서 해당 페이지 구조체의 정보들을 저장함.
// vm_initializer..함수의 인자 참고
// 부모 페이지의 정보를 저장한뒤 자식이 가질 새로운 페이지 생성.
// 생성 위해서 부모 페이지의 타입 먼저 검사. 부모 페이지가 UNINIT페이지인 경우와 그렇지 않은 경우 ->
// UNINIT아닌 경우, setup_sstack에서 했던것처럼 페이지를 생성한 뒤 바로 해당 페이지의 타입에 맞는 Initializer 호출., 그리고나서 부모의 물리 페이지 정보를 자식에게도 복사.
// 모든 함수가 정상적으로 되었다면 return true;

// SPT를 복사하는 함수 (자식 프로세스가 부모 프로세스의 실행 컨텍스트를 상속해야 할 때 (즉, fork() 시스템 호출이 사용될 때) 사용)
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	// : 보조 페이지 테이블을 src에서 dst로 복사합니다.
	// : src의 각 페이지를 순회하고 dst에 해당 entry의 사본을 만듭니다.
	// : uninit page를 할당하고 그것을 즉시 claim해야 합니다.
	// src로부터 dst보조테이블을 복사한다.
	struct hash_iterator i;

	hash_first(&i, &src->spt_hash);
	while (hash_next(&i))
	{
		// enum vm_type type, void *upage, bool writable,
		// vm_initializer *init, void *aux
		struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = p->operations->type;
		void *upage = p->va;
		bool writable = p->writable;
		vm_initializer *init = p->uninit.init;
		// malloc을 한 뒤에 넘겨야 한다.
		//  uninit 타입인 경우는 aux 복사, 나머지 타입인 경우에는.. 생각을 해봐라 모르겠는데? 알려줘 답주세요 답답답 dap
		//  최종 진화 타입여부에 따라타입을 나눠서 어떤것을 복제 떠야 하는지 생각해봐라.

		// bool (*page_initializer)(struct page *, enum vm_type, void *);
		if (type == VM_UNINIT)
		// 부모 페이지가 초기화가 안된경우,
		{
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, p->uninit.init, p->uninit.aux);
			// 왜요 ? 부모가 초기화가 안된 상태, 초기화를 해주고 다시 continue 해서 반복문 돌고, 다시 부모정보를 받아와서
			//  밑에서 초기화가 된 상태로 else 를 지나서 자식에게 복사가 된다.
			// 다시 위로 올라감.
			continue;
		}

		if (type == VM_FILE)
		{
			struct lazy_load_arg *lazy_load_arg = malloc(sizeof(struct lazy_load_arg));
			lazy_load_arg->file = p->file.file;
			lazy_load_arg->ofs = p->file.ofs;
			lazy_load_arg->read_bytes = p->file.read_bytes;
			if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, NULL, lazy_load_arg))
			{
				return false;
			}
			struct page *file_page = spt_find_page(dst, upage);
			file_backed_initializer(file_page, type, NULL);
			pml4_set_page(thread_current()->pml4, file_page->va, p->frame->kva, p->writable);
			continue;
		}

		// 부모의 페이지가 초기화가 된 경우,
		//  자식의 페이지를 생성.

		if (!vm_alloc_page(type, upage, writable))
			return false;

		if (!vm_claim_page(upage))
			// 자식페이지를 물리주소랑 맵핑.
			return false;
		if (type != VM_UNINIT)
		{

			struct page *child_page = spt_find_page(dst, upage); // dst 보조테이블에서 현재 복사할 가상주소 upage에 해당하는 자식페이지를 찾음
			memcpy(child_page->frame->kva, p->frame->kva, PGSIZE);
		}
	}
	return true;
}

void hash_page_destroy(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
	// TODO - 지우면 에러 헤결
	// free(page);
}
/* Free the resource hold by the supplemental page table */
// SPT가 보유하고 있던 모든 리소스를 해제하는 함수 (process_exit(), process_cleanup()에서 호출)
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* : Destroy all the supplemental_page_table hold by thread and
	 * : writeback all the modified contents to the storage. */
	// todo: 페이지 항목들을 순회하며 테이블 내의 페이지들에 대해 destroy(page)를 호출
	// hash_clear(&spt->spt_hash, hash_page_destroy); // 해시 테이블의 모든 요소를 제거

	/** hash_destroy가 아닌 hash_clear를 사용해야 하는 이유
	 * 여기서 hash_destroy 함수를 사용하면 hash가 사용하던 메모리(hash->bucket) 자체도 반환한다.
	 * process가 실행될 때 hash table을 생성한 이후에 process_clean()이 호출되는데,
	 * 이때는 hash table은 남겨두고 안의 요소들만 제거되어야 한다.
	 * 따라서, hash의 요소들만 제거하는 hash_clear를 사용해야 한다.
	 */

	// todo🚨: 모든 수정된 내용을 스토리지에 기록
	hash_clear(&spt->spt_hash, hash_page_destroy);
	// 끝? ㅇㅇ
}
