# TEST
Using to test code
https://www.youtube.com/playlist?list=PLRm-bXfDhjHAgXetBRhb09zTARpgxYw2u


**1. ** Cách tạo shortcut Artemis ra Desktop trên Raspberry Pi
1) Mở Terminal.

2) Tạo file mới ngoài Desktop:

    nano ~/Desktop/artemis.desktop


3)  Thêm nội dung sau vào file:

   Desktop Entry]
    Name=Artemis
    Comment=Mở Artemis Web
    Exec=chromium-browser http://10.0.108.10/artemis/
    Icon=chromium-browser
    Terminal=false
    Type=Application


Nếu bạn dùng Firefox thì đổi Exec=firefox http://10.0.108.10/artemis/.

Lưu lại (Ctrl+O, Enter, rồi Ctrl+X).

5) Cấp quyền chạy cho file:

chmod +x ~/Desktop/artemis.desktop


Ra ngoài Desktop sẽ thấy icon Artemis. Nhấp đúp vào → mở ngay trang web.


https://docs.google.com/document/d/1Rb8UaDynwUrwcfDJrvctgYneU0n5FE1Cy_KCjjXtrVY/edit?usp=sharing
