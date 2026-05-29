import pygame
import sys
import time

# Initialize pygame
pygame.init()

# Screen dimensions
WIDTH, HEIGHT = 1400, 900
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Dashboard Emulator")

# Colors
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (100, 100, 100)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
YELLOW = (255, 255, 0)
ORANGE = (255, 165, 0)

# Font
font = pygame.font.SysFont(None, 36)
small_font = pygame.font.SysFont(None, 24)

class DashboardCommunicator:
    def __init__(self):
        self.left_blinker = False
        self.right_blinker = False
        self.brake = False
        self.speed = 0
        
        # Button rectangles
        self.left_blinker_rect = pygame.Rect(300, 50, 100, 100)
        self.right_blinker_rect = pygame.Rect(620, 50, 100, 100)
        self.brake_rect = pygame.Rect(450, 50, 100, 100)
        
        # Status indicators
        self.status_rect = pygame.Rect(50, 400, 900, 100)
        
        # Data update timer
        self.last_update = time.time()
        
    def update_data(self, speed=None, left_blinker=None, right_blinker=None, brake=None):
        """Update dashboard data"""
        if speed is not None:
            self.speed = max(0, min(200, speed))  # Speed limit 0-200 km/h
        if left_blinker is not None:
            self.left_blinker = left_blinker
        if right_blinker is not None:
            self.right_blinker = right_blinker
        if brake is not None:
            self.brake = brake
            
        self.last_update = time.time()
        
    def draw_dashboard(self):
        """Draw the entire dashboard"""
        screen.fill(BLACK)
        
        # Draw title
        title = font.render("Dashboard Emulator", True, WHITE)
        screen.blit(title, (WIDTH//2 - title.get_width()//2, 10))
        
        # Draw main dashboard area
        pygame.draw.rect(screen, GRAY, (20, 20, WIDTH-40, HEIGHT-40), 2)
        
        # Draw speed area
        pygame.draw.rect(screen, (50, 50, 50), (50, 50, 200, 300))
        pygame.draw.rect(screen, WHITE, (50, 50, 200, 300), 2)
        speed_text = font.render("SPEED", True, WHITE)
        screen.blit(speed_text, (150 - speed_text.get_width()//2, 60))
        
        # Draw speed value
        speed_value = font.render(f"{self.speed} km/h", True, WHITE)
        screen.blit(speed_value, (150 - speed_value.get_width()//2, 150))
        
        # Draw blinker buttons
        # Left blinker
        pygame.draw.rect(screen, (50, 50, 50), self.left_blinker_rect)
        pygame.draw.rect(screen, WHITE, self.left_blinker_rect, 2)
        blinker_text = font.render("LEFT", True, WHITE)
        screen.blit(blinker_text, (self.left_blinker_rect.centerx - blinker_text.get_width()//2, 
                                  self.left_blinker_rect.top + 10))
        if self.left_blinker:
            pygame.draw.rect(screen, YELLOW, self.left_blinker_rect, 3)
            
        # Right blinker
        pygame.draw.rect(screen, (50, 50, 50), self.right_blinker_rect)
        pygame.draw.rect(screen, WHITE, self.right_blinker_rect, 2)
        blinker_text = font.render("RIGHT", True, WHITE)
        screen.blit(blinker_text, (self.right_blinker_rect.centerx - blinker_text.get_width()//2, 
                                  self.right_blinker_rect.top + 10))
        if self.right_blinker:
            pygame.draw.rect(screen, YELLOW, self.right_blinker_rect, 3)
            
        # Draw brake button
        pygame.draw.rect(screen, (50, 50, 50), self.brake_rect)
        pygame.draw.rect(screen, WHITE, self.brake_rect, 2)
        brake_text = font.render("BRAKE", True, WHITE)
        screen.blit(brake_text, (self.brake_rect.centerx - brake_text.get_width()//2, 
                                self.brake_rect.top + 10))
        if self.brake:
            pygame.draw.rect(screen, RED, self.brake_rect, 3)
            brake_active = font.render("ON", True, RED)
            screen.blit(brake_active, (self.brake_rect.centerx - brake_active.get_width()//2, 
                                      self.brake_rect.bottom - 40))
            
        # Draw instructions
        instructions = [
            "Controls:",
            "Speed: Use mouse wheel or up/down keys",
            "Left Blinker: L key",
            "Right Blinker: R key", 
            "Brake: B key",
            "Quit: ESC key"
        ]
        
        for i, text in enumerate(instructions):
            text_surface = small_font.render(text, True, WHITE)
            screen.blit(text_surface, (20, HEIGHT - 150 + i * 20))
            
    def handle_events(self):
        """Handle input events"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
                
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    return False
                elif event.key == pygame.K_l:
                    self.left_blinker = not self.left_blinker
                elif event.key == pygame.K_r:
                    self.right_blinker = not self.right_blinker
                elif event.key == pygame.K_b:
                    self.brake = not self.brake
                elif event.key == pygame.K_UP:
                    self.speed = min(200, self.speed + 1)
                elif event.key == pygame.K_DOWN:
                    self.speed = max(0, self.speed - 1)
                elif event.key == pygame.K_PAGEUP:
                    self.speed = min(200, self.speed + 10)
                elif event.key == pygame.K_PAGEDOWN:
                    self.speed = max(0, self.speed - 10)
                    
            if event.type == pygame.MOUSEBUTTONDOWN:
                # Check if blinker buttons were clicked
                if self.left_blinker_rect.collidepoint(event.pos):
                    self.left_blinker = not self.left_blinker
                elif self.right_blinker_rect.collidepoint(event.pos):
                    self.right_blinker = not self.right_blinker
                    
                # Check if brake button was clicked
                if self.brake_rect.collidepoint(event.pos):
                    self.brake = not self.brake
                    
            # Handle mouse wheel for speed control
            if event.type == pygame.MOUSEBUTTONDOWN:
                if event.button == 4:  # Mouse wheel up
                    self.speed = min(200, self.speed + 10)
                elif event.button == 5:  # Mouse wheel down
                    self.speed = max(0, self.speed - 10)
                    
        return True

def main():
    emulator = DashboardCommunicator()
    clock = pygame.time.Clock()
    
    running = True
    while running:
        running = emulator.handle_events()
        emulator.draw_dashboard()
        pygame.display.flip()
        clock.tick(30)
        
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()