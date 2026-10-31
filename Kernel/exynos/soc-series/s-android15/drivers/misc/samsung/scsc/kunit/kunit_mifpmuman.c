#define wait_for_completion_timeout(args...)	(0)

static void mifpmu_isr(int irq, void *data);
void (*fp_mifpmu_isr)(int irq, void *data) = &mifpmu_isr;

static void mifpmu_cmd_th_to_string(u32 cmd, char *buf, u8 buffer_size);
void (*fp_mifpmu_cmd_th_to_string)(u32 cmd, char *buf, u8 buffer_size) = &mifpmu_cmd_th_to_string;

static void mifpmu_cmd_fh_to_string(u32 cmd, char *buf, u8 buffer_size);
void (*fp_mifpmu_cmd_fh_to_string)(u32 cmd, char *buf, u8 buffer_size) = &mifpmu_cmd_fh_to_string;
