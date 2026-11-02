        .text
        .global cd_worker_blob,cd_worker_blob_end
        .align 2
cd_worker_blob:
        .incbin "cd-worker.bin"
cd_worker_blob_end:
