import type { ButtonHTMLAttributes, ReactNode } from 'react';
import { joinClasses } from '../utils/joinClasses';

type ButtonProps = ButtonHTMLAttributes<HTMLButtonElement> & { children: ReactNode };

export function Button({ children, className, ...rest }: ButtonProps) {
    return (
        <button
            type="button"
            className={joinClasses(
                'button min-w-20 cursor-pointer rounded border border-border bg-surface px-3 py-[5px] hover:enabled:bg-surface-hover disabled:cursor-default disabled:opacity-50',
                className,
            )}
            {...rest}
        >
            {children}
        </button>
    );
}
