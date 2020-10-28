
//////// Somwhere else in the code, outside the function provided /////////
int8_t current_state = 0;
int8_t acum = 0;
bool is_negative = false;

int8_t preparePrint(void) {
    int8_t next_index = 0;
    appData.cdcWriteBuffer[next_index++] = '=';
    if (is_negative) {
        appData.cdcWriteBuffer[next_index++] = '-';
        --acum;
    } else {
        ++acum;
    }
    if (acum >= 100) {
        appData.cdcWriteBuffer[next_index++] = (acum / 100) + '0';
        acum %= 100;
        appData.cdcWriteBuffer[next_index++] = (acum / 10) + '0';
        appData.cdcWriteBuffer[next_index++] = (acum % 10) + '0';
    } else if (acum >= 10) {
        appData.cdcWriteBuffer[next_index++] = (acum / 10) + '0';
        appData.cdcWriteBuffer[next_index++] = (acum % 10) + '0';
    } else {
        appData.cdcWriteBuffer[next_index++] = acum + '0';
    }
    appData.cdcWriteBuffer[next_index++] = ' ';
    acum = 0;
    is_negative = false;
    current_state = 0;
    return next_index;
}


/////////////////// Section of the function given  ///////////////////////

case APP_STATE_SCHEDULE_READ:
    appData.state = APP_STATE_WAIT_FOR_READ_COMPLETE;
    if (appData.isReadComplete == true) {
        appData.isReadComplete = false;
        appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
        USB_DEVICE_CDC_Read(USB_DEVICE_CDC_INDEX_0, &appData.readTransferHandle,
                            appData.cdcReadBuffer, APP_READ_BUFFER_SIZE);
        if (appData.readTransferHandle == USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID) {
            appData.state = APP_STATE_ERROR;
            break;
        }
    }
    break;
case APP_STATE_SCHEDULE_WRITE:
    appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
    appData.isWriteComplete = false;
    appData.state = APP_STATE_WAIT_FOR_WRITE_COMPLETE;

    int8_t extra_bytes = 0;
    for (i = 0; i < appData.numBytesRead; i++) {
        if ((appData.cdcReadBuffer[i] != 0x0A) && (appData.cdcReadBuffer[i] != 0x0D)) {
            const char current_char = appData.cdcReadBuffer[i];
            if (current_char == '-' && current_state = 0) {
                is_negative = true;
                current_state = 1;
                appData.cdcWriteBuffer[extra_bytes + i] = current_read;  // to echo the current -
            } else if (current_read >= '0' && current_read <= '9') {
                if (acum >= 10) {
                    extra_bytes = preparePrint();
                }
                if (current_state == 0 || (is_negative && current_state == 1)) {
                    acum = 0;
                }
                acum *= 10;
                acum += current_read - '0';
                current_state = 1;
                appData.cdcWriteBuffer[extra_bytes + i] = current_read;  // to echo the current digit
            } else {               // Invalid character is not processed nor echoed
                if (current_state != 0) {
                    extra_bytes = preparePrint();
                }
            }
        }
    }
    USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0, &appData.writeTransferHandle,
                         appData.cdcWriteBuffer,
                         appData.numBytesRead + extra_bytes,  // print the extra bytes
                         USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
    }
    break;