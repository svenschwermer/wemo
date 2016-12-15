################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
instance.c \
instanceIF.c \
udp_server.c \
src/utility.c \
src/wemotest.c 

OBJS += \
instance.o \
instanceIF.o \
udp_server.o \
utility.o \
wemotest.o 

C_DEPS += \
instance.d \
instanceIF.d \
udp_server.d \
utility.d \
wemotest.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


