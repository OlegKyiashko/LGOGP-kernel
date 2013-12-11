cmd_.tmp_kallsyms1.o := /home/serg/linaro/bin/arm-linux-gnueabihf-gcc -Wp,-MD,./..tmp_kallsyms1.o.d -D__ASSEMBLY__ -mabi=aapcs-linux -mno-thumb-interwork -funwind-tables  -D__LINUX_ARM_ARCH__=7 -mcpu=cortex-a15  -include asm/unified.h -msoft-float    -nostdinc -isystem /home/serg/linaro/bin/../lib/gcc/arm-linux-gnueabihf/4.8.3/include -I/home/serg/gpro4.4/arch/arm/include -Iarch/arm/include/generated -Iinclude  -I/home/serg/gpro4.4/include -include /home/serg/gpro4.4/include/linux/kconfig.h -D__KERNEL__ -mlittle-endian -I/home/serg/gpro4.4/arch/arm/mach-msm/include    -c -o .tmp_kallsyms1.o .tmp_kallsyms1.S

source_.tmp_kallsyms1.o := .tmp_kallsyms1.S

deps_.tmp_kallsyms1.o := \
  /home/serg/gpro4.4/arch/arm/include/asm/unified.h \
    $(wildcard include/config/arm/asm/unified.h) \
    $(wildcard include/config/thumb2/kernel.h) \
  /home/serg/gpro4.4/arch/arm/include/asm/types.h \
  /home/serg/gpro4.4/include/asm-generic/int-ll64.h \
  arch/arm/include/generated/asm/bitsperlong.h \
  /home/serg/gpro4.4/include/asm-generic/bitsperlong.h \
    $(wildcard include/config/64bit.h) \

.tmp_kallsyms1.o: $(deps_.tmp_kallsyms1.o)

$(deps_.tmp_kallsyms1.o):
