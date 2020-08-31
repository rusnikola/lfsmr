# Hyaline Reclamation

The is the reclamation implementation itself. For complete instructions,
see ../README.md in the parent directory.

* Source code license

	Copyright (c) 2018 Ruslan Nikolaev. All Rights Reserved.

	The Hyaline code is distributed under the 2-Clause BSD license.

* Include the following header files from your program

	lfsmr.h (Hyaline)

	lfbsmr.h (Hyaline-S)

	lfsmro.h (Hyaline-1)

	lfbsmro.h (Hyaline-1S)

* Compatibility

	For Hyaline and Hyaline-S, you can use both gcc and clang for
	x86, x86-64, ARM32, PPC, and MIPS. For AArch64, only clang is
	currently supported. For Hyaline-1 and Hyaline-1S, there
	should be no such restriction.

	However, if you want to mix this code with C++11, gcc may not
	work since we use C11 which itself is not part of the
	C++11 standard. Consequently, only clang will currently
	work in this case.

	For x86-64, you must specify the -mcx16 (cmpxchg16b) compiler
	flag for Hyaline and Hyaline-S.
	For ARM32, you must choose the CPU architecture MPCore or higher.

	Click [here](http://releases.llvm.org/download.html)
	to download the latest clang for Linux, FreeBSD, Windows, and macOS.

* Usage example for Hyaline and Hyaline-1

```
    #define SMR_ORDER 6                  // log2(k)
    #define SMR_NUM   (1U << SMR_ORDER)  // k = 2^6 = 64 slots
    #define SMR_BATCH (SMR_NUM+1)        // batch size, at least (k+1)

    // Linked list header
    struct list_t
    {
        union {
            _Alignas(LFSMR_ALIGN) char _smr[LFSMR_SIZE(SMR_NUM)];
            struct lfsmr smr;
        };
        ...
    };

    // Linked list node
    struct node_t
    {
        struct lfsmr_node smr;
        ...
    };

    // Initialization
    __thread unsigned long slot_num = thread_id() % SMR_NUM;
    __thread lfsmr_batch_t smr_batch;
    lfsmr_batch_init(&smr_batch)

    // Usage
    lfsmr_handle_t handle;
    lfsmr_enter(&list->smr, slot_num, &handle, 0, LF_DONTCHECK);
    ...
    lfsmr_retire(&list->smr, SMR_ORDER, &node->smr,
                 data_free_node, 0, &smr_batch, SMR_BATCH);
    ...
    lfsmr_leave(&list->smr, slot_num, SMR_ORDER, handle,
	        data_free_node, 0, LF_DONTCHECK);

    // Deallocation method
    void data_free_node(struct lfsmr *hdr, struct lfsmr_node *smrnode)
    {
        struct node_t *node = container_of(smrnode, struct node_t, smr);
        free(node);
    }

    // Finalizing batches with dummy nodes
    while (!lfsmr_batch_empty(&smr_batch)) {
        struct lfsmr_node *node = (struct lfsmr_node *)
	                     malloc(sizeof(struct lfsmr_node));
        lfsmr_retire(&list->smr, SMR_ORDER, node, data_free_node,
                     0, &smr_batch, SMR_BATCH);
    }
```

* Usage example for Hyaline-S

```
    // The slot_num value can change
    lfbsmr_handle_t handle;
    lfbsmr_enter(&list->smr, &slot_num, SMR_ORDER, &handle,
                 0, LF_DONTCHECK);
    ...

    // All dereferences must be wrapped:
    node = (node_t *)lfbsmr_deref(&list->smr, slot_num,
              (uintptr_t *)&prev->next);
```

* The actual implementation for Hyaline is in *bits*:

	bits/lfbsmr_cas1.h (Hyaline-1S)

	bits/lfbsmr_cas2.h (Hyaline-S)

	bits/lfbsmr_common.h (common Hyaline-1S/Hyaline-S code)

	bits/lfsmr_cas1.h (Hyaline-1)

	bits/lfsmr_cas2.h (Hyaline)

	bits/lfsmr_common.h (common Hyaline-1/Hyaline code)

	bits/mips.h (MIPS LL/SC implementation)

	bits/ppc.h (PPC LL/SC implementation)
