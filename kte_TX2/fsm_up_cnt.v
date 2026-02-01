===============================================================================
module fsm_up_cnt(
    input wire clk, rst,
    output reg [3:0] count,
    output reg [6:0] led_7
);
    localparam [1:0]
        state_rst    = 2'b00,
        state_up_cnt = 2'b01;

    reg [1:0] current_state, next_state;

    // Next-state logic
    always @(*) begin
        case (current_state)
            state_rst:    next_state = state_up_cnt;
            state_up_cnt: next_state = state_up_cnt;
            default:      next_state = state_rst;
        endcase
    end

    // State register + counter
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            current_state <= state_rst;
            count <= 4'd0;
        end else begin
            current_state <= next_state;
            case (next_state)
                state_rst:    count <= 4'd0;
                state_up_cnt: begin
                    if (count == 4'd9)
                        count <= 4'd0;
                    else
                        count <= count + 1'b1;
                end
            endcase
        end
    end

    // 7-seg decoder
    always @(*) begin
        case (count)
            4'd0: led_7 = 7'b1000000;
            4'd1: led_7 = 7'b1111001;
            4'd2: led_7 = 7'b0100100;
            4'd3: led_7 = 7'b0110000;
            4'd4: led_7 = 7'b0011001;
            4'd5: led_7 = 7'b0010010;
            4'd6: led_7 = 7'b0000010;
            4'd7: led_7 = 7'b1111000;
            4'd8: led_7 = 7'b0000000;
            4'd9: led_7 = 7'b0010000;
            default: led_7 = 7'b1111111;
        endcase
    end
endmodule
*****************************************************************************

`timescale 1ns/1ps
module tb_fsm_up_cnt;

  reg clk, rst;
  wire [3:0] count;
  wire [6:0] led_7;

  // Kết nối với module chính
  fsm_up_cnt uut (
    .clk(clk),
    .rst(rst),
    .count(count),
    .led_7(led_7)
  );

  // Tạo xung clock 10ns
  initial begin
    clk = 0;
    forever #5 clk = ~clk;
  end

  // In trạng thái mô phỏng ra màn hình
  initial begin
    $display("=== BẮT ĐẦU MÔ PHỎNG ===");
    $display("Thời gian\tclk\trst\tcount\tled_7");
    $monitor("%0t\t%b\t%b\t%d\t%b", $time, clk, rst, count, led_7);
  end

  // Kịch bản mô phỏng
  initial begin
    // Reset ban đầu
    rst = 1;
    repeat(2) @(posedge clk);
    rst = 0;

    // Đếm tiến từ s0 -> s9
    $display("== ĐẾM TIẾN TỪ s0 -> s9 ==");
    repeat(12) @(posedge clk);

    // Reset giữa chừng
    $display("== RESET GIỮA CHỪNG ==");
    rst = 1; @(posedge clk);
    rst = 0;
    repeat(6) @(posedge clk);

    // Kết thúc mô phỏng
    $display("== KẾT THÚC MÔ PHỎNG ==");
    $stop;
  end

endmodule


===================================================================================
