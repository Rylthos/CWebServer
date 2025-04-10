flags = -g -Wall -Wpedantic -O1 -MD

objFolder := build
srcFolder := src
SRCS := $(wildcard $(srcFolder)/*.c)
OBJS := $(addprefix $(objFolder)/, $(patsubst %.c, %.o, $(notdir $(SRCS))))

all: web

clean:
	@rm -rf build
	@rm web

$(objFolder):
	@echo Creating $(objFolder) folder
	@mkdir -p $(objFolder)

web: $(OBJS)
	@echo Compiling $@
	gcc -o $@ $^

$(OBJS): | $(objFolder)

$(OBJS): $(objFolder)/%.o: $(srcFolder)/%.c
	@echo Building $<
	gcc $(flags) -c $< -o $@

-include $(OBJS:.o=.d)
